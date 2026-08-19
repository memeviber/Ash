open Ast

module SMap = Map.Make (String)
module SSet = Set.Make (String)

type env = {
  vars : typ SMap.t;
  funcs : (typ list * typ) SMap.t;
  structs : (typ SMap.t) SMap.t;
  enums : unit SMap.t;
    enum_values : typ SMap.t;
  consts : unit SMap.t;
  scope_vars : SSet.t;
  owned : SSet.t;
  moved : SSet.t;
  borrow_counts : int SMap.t;
  borrow_sources : string SMap.t
}

let rec equal_typ a b =
  match a, b with
  | TInt, TInt | TBool, TBool | TInt, TBool | TBool, TInt
  | TChar, TChar | TString, TString | TFloat, TFloat | TDouble, TDouble | TFloat, TDouble | TDouble, TFloat
  | TLong, TLong | TLongLong, TLongLong | TVoid, TVoid -> true
  | TVoid, TInt | TInt, TVoid -> true
  | TPtr x, TPtr y -> equal_typ x y
  | TArray (x, n), TArray (y, m) -> n = m && equal_typ x y
  | TDynArray x, TDynArray y -> equal_typ x y
  | TNamed x, TNamed y -> String.equal x y
  | TParam x, TParam y -> String.equal x y
  | TGeneric (na, aa), TGeneric (nb, bb) ->
      String.equal na nb && List.length aa = List.length bb && List.for_all2 equal_typ aa bb
  | TFunPtr (ap, ar), TFunPtr (bp, br) ->
      List.length ap = List.length bp && List.for_all2 equal_typ ap bp && equal_typ ar br
  | _ -> false

let is_integer_like = function
  | TInt | TBool | TChar | TLong | TLongLong -> true
  | _ -> false

let is_numeric = function
  | TInt | TBool | TChar | TFloat | TDouble | TLong | TLongLong -> true
  | _ -> false

let is_scalar = function
  | TInt | TBool | TChar | TFloat | TDouble | TLong | TLongLong | TPtr _ | TFunPtr _ -> true
  | _ -> false

let numeric_result_type a b =
  match a, b with
  | TDouble, _ | _, TDouble -> TDouble
  | TFloat, _ | _, TFloat -> TFloat
  | TLongLong, _ | _, TLongLong -> TLongLong
  | TLong, _ | _, TLong -> TLong
  | _ -> TInt

let integer_result_type a b =
  match a, b with
  | TLongLong, _ | _, TLongLong -> TLongLong
  | TLong, _ | _, TLong -> TLong
  | _ -> TInt

let compatible_typ a b =
  equal_typ a b ||
  (is_numeric a && is_numeric b) ||
  match a, b with
  | TParam _, _ | _, TParam _ -> true
  | TPtr _, TPtr TVoid | TPtr TVoid, TPtr _ -> true
  | _ -> false

let const_int = function
  | Int k -> Some k
  | Binop (Sub, Int 0, Int k) -> Some (-k)
  | _ -> None

let rec substitute_typ subst = function
  | TParam name -> (match SMap.find_opt name subst with Some t -> t | None -> TParam name)
  | TPtr t -> TPtr (substitute_typ subst t)
  | TArray (t, n) -> TArray (substitute_typ subst t, n)
  | TDynArray t -> TDynArray (substitute_typ subst t)
  | TFunPtr (args, ret) -> TFunPtr (List.map (substitute_typ subst) args, substitute_typ subst ret)
  | TGeneric (name, args) -> TGeneric (name, List.map (substitute_typ subst) args)
  | t -> t

let rec unify_typ subst formal actual =
  match formal with
  | TParam name ->
      (match SMap.find_opt name subst with
       | None -> Ok (SMap.add name actual subst)
       | Some previous -> if equal_typ previous actual then Ok subst else Error ("conflicting type inference for " ^ name))
  | TPtr f -> (match actual with TPtr a -> unify_typ subst f a | _ -> Error "generic pointer argument mismatch")
  | TArray (f, fn) -> (match actual with TArray (a, an) when fn = an -> unify_typ subst f a | _ -> Error "generic array argument mismatch")
  | TDynArray f -> (match actual with TDynArray a -> unify_typ subst f a | _ -> Error "generic dynamic-array argument mismatch")
  | TGeneric (fn, fs) ->
      (match actual with
       | TGeneric (an, as_) when String.equal fn an && List.length fs = List.length as_ ->
           List.fold_left2 (fun acc f a -> Result.bind acc (fun s -> unify_typ s f a)) (Ok subst) fs as_
       | _ -> Error "generic application argument mismatch")
  | TFunPtr (fps, fr) ->
      (match actual with
       | TFunPtr (aps, ar) when List.length fps = List.length aps ->
           let s = List.fold_left2 (fun acc f a -> Result.bind acc (fun s0 -> unify_typ s0 f a)) (Ok subst) fps aps in
           Result.bind s (fun s0 -> unify_typ s0 fr ar)
       | _ -> Error "generic function-pointer argument mismatch")
  | _ -> if compatible_typ formal actual then Ok subst else Error "generic argument type mismatch"

let rec equal_array_elem_typ a b =
  match a, b with
  | TFloat, TDouble | TDouble, TFloat -> true
  | TPtr x, TPtr y -> equal_array_elem_typ x y
  | TNamed x, TNamed y -> String.equal x y
  | _ -> equal_typ a b

let rec string_of_typ = function
  | TInt -> "int"
  | TBool -> "bool"
  | TChar -> "char"
  | TString -> "string"
  | TFloat -> "float"
  | TDouble -> "double"
  | TLong -> "long"
  | TLongLong -> "long long"
  | TVoid -> "void"
  | TPtr _ -> "pointer"
  | TArray _ -> "fixed array"
  | TDynArray t -> "array<" ^ string_of_typ t ^ ">"
  | TFunPtr _ -> "function pointer"
  | TNamed n -> n
  | TParam n -> n
  | TGeneric (n, args) -> n ^ "<" ^ String.concat "," (List.map string_of_typ args) ^ ">"

let check program =
  let function_names = List.map (fun f -> f.name) program.functions @ List.map (fun (name, _, _) -> name) program.generic_functions in
  let reserved_names =
    List.fold_left (fun s name -> SSet.add name s) SSet.empty
      ["printf"; "memory_alloc"; "memory_resize"; "memory_free"; "alloc_ints"; "free_ints"; "grow_ints";
       "open_file"; "read_char"; "close_file"; "write_char"; "write_string"; "write_int";
       "runtime_string_concat"; "basalt_track"; "basalt_release"; "basalt_memory_alloc";
       "basalt_memory_resize"; "basalt_memory_free"; "basalt_panic"; "basalt_checked_bytes";
       "basalt_find"; "basalt_validate"; "basalt_cleanup"; "basalt_inc_find"; "basalt_inc_add";
       "basalt_inc_strdup"; "basalt_inc_realpath"; "basalt_inc_join"; "basalt_include_line_mode";
       "basalt_include_close"; "basalt_include_open_root"; "basalt_include_open_line";
       "basalt_include_last_status"; "basalt_include_reset_session";
       "malloc"; "calloc"; "realloc"; "free"; "memcpy"; "memset"; "strlen"; "strrchr";
       "fopen"; "fclose"; "fgetc"; "fputc"; "fputs"; "fprintf"; "exit"; "atexit"]
  in
  let duplicate_name_error names kind =
    let rec loop seen = function
      | [] -> None
      | name :: rest ->
          if SSet.mem name seen then Some ("duplicate " ^ kind ^ " " ^ name)
          else loop (SSet.add name seen) rest
    in
    loop SSet.empty names
  in
  let mangled_name_error names =
    let rec loop seen = function
      | [] -> None
      | name :: rest ->
          let emitted = Ast.c_symbol_name name in
          (match SMap.find_opt emitted seen with
           | Some previous -> Some ("C symbol collision: " ^ previous ^ " and " ^ name ^ " both emit " ^ emitted)
           | None -> loop (SMap.add emitted name seen) rest)
    in
    loop SMap.empty names
  in
  let reserved_name_error names =
    List.find_map (fun name -> if SSet.mem name reserved_names then Some ("reserved runtime function name " ^ name) else None) names
  in
  let declaration_error =
    match duplicate_name_error function_names "function" with
    | Some e -> Some e
    | None ->
        (match mangled_name_error function_names with
         | Some e -> Some e
         | None -> reserved_name_error function_names)
  in
  let funcs =
    let user_funcs =
      List.fold_left
        (fun m f -> SMap.add f.name (List.map snd f.params, f.return_type) m)
        SMap.empty program.functions
    in
    let declared_externs =
      List.fold_left
        (fun m f -> if SMap.mem f.name m then m else SMap.add f.name (List.map snd f.params, f.return_type) m)
        user_funcs program.externs
    in
    let builtins = Ast.builtin_funcs in
    List.fold_left (fun m (name, sig_) -> if SMap.mem name m then m else SMap.add name sig_ m) declared_externs builtins
  in
  let structs =
    List.fold_left
      (fun m (name, fields) ->
        let fs = List.fold_left (fun fm (field, ty) -> SMap.add field ty fm) SMap.empty fields in
        SMap.add name fs m)
      SMap.empty program.structs
  in
  let enums =
    List.fold_left (fun m (name, _) -> SMap.add name () m) SMap.empty program.enums
  in
  let enum_values =
    List.fold_left
      (fun m (name, values) ->
        List.fold_left (fun vm value -> SMap.add value (TNamed name) vm) m values)
      SMap.empty program.enums
  in
  let generic_structs =
    List.fold_left
      (fun m (name, params, fields) ->
        let fs = List.fold_left (fun fm (field, ty) -> SMap.add field ty fm) SMap.empty fields in
        SMap.add name (params, fs) m)
      SMap.empty program.generic_structs
  in
  let generic_functions =
    List.fold_left
      (fun m (name, params, f) -> SMap.add name (params, (List.map snd f.params, f.return_type)) m)
      SMap.empty program.generic_functions
  in
  let named_type_exists name = SMap.mem name structs || SMap.mem name enums || SMap.mem name generic_structs in
  (* A recursive struct is valid through a pointer, but not by value. *)
  let rec value_cycle visiting name =
    if SSet.mem name visiting then true
    else
      match SMap.find_opt name structs with
      | None -> false
      | Some fields ->
          let visiting = SSet.add name visiting in
          SMap.exists (fun _ ty -> value_cycle_in_type visiting ty) fields
  and value_cycle_in_type visiting = function
    | TInt | TBool | TChar | TString | TFloat | TDouble | TLong | TLongLong | TVoid | TPtr _ | TParam _ -> false
    | TArray (ty, _) -> value_cycle_in_type visiting ty
    | TDynArray _ -> false
    | TFunPtr (args, ret) -> List.exists (value_cycle_in_type visiting) (ret :: args)
    | TGeneric (_, args) -> List.exists (value_cycle_in_type visiting) args
    | TNamed name -> value_cycle visiting name
  in
  let check_type ty =
    let rec go = function
      | TInt | TBool | TChar | TString | TFloat | TDouble | TLong | TLongLong | TVoid | TParam _ -> Ok ()
      | TPtr t | TArray (t, _) | TDynArray t -> go t
      | TGeneric (name, args) ->
          (match SMap.find_opt name generic_structs with
           | None -> Error ("unknown generic type " ^ name)
           | Some (params, _) when List.length params <> List.length args ->
               Error ("generic type " ^ name ^ " expects " ^ string_of_int (List.length params) ^ " arguments")
           | Some _ -> List.fold_left (fun acc t -> Result.bind acc (fun () -> go t)) (Ok ()) args)
      | TFunPtr (args, ret) ->
          let rec all = function
            | [] -> go ret
            | x :: xs -> Result.bind (go x) (fun () -> all xs)
          in
          all args
      | TNamed name ->
          if named_type_exists name then Ok ()
          else Error ("unknown named type " ^ name)
    in
    go ty
  in
  let check_struct_cycles () =
    let rec check_fields = function
      | [] -> Ok ()
      | (name, fields) :: rest ->
          let bad =
            List.find_opt
              (fun (_field, ty) ->
                match ty with
                | TNamed target when value_cycle (SSet.singleton name) target -> true
                | TArray _ when value_cycle_in_type (SSet.singleton name) ty -> true
                | _ -> false)
              fields
          in
          (match bad with
           | Some (field, _) ->
               Error ("recursive struct field by value: " ^ name ^ "." ^ field)
           | None -> check_fields rest)
    in
    check_fields program.structs
  in
  let field_type base field =
    match base with
    | TDynArray _ when field = "len" || field = "cap" -> Ok TInt
    | TDynArray _ -> Error ("unknown dynamic array field " ^ field)
    | _ ->
        let named =
          match base with
          | TNamed name -> Some (name, SMap.empty)
          | TPtr (TNamed name) -> Some (name, SMap.empty)
          | TGeneric (name, args) ->
              (match SMap.find_opt name generic_structs with
               | None -> None
               | Some (params, _) ->
                   if List.length params <> List.length args then None
                   else Some (name, List.fold_left2 (fun s p a -> SMap.add p a s) SMap.empty params args))
          | TPtr (TGeneric (name, args)) ->
              (match SMap.find_opt name generic_structs with
               | None -> None
               | Some (params, _) ->
                   if List.length params <> List.length args then None
                   else Some (name, List.fold_left2 (fun s p a -> SMap.add p a s) SMap.empty params args))
          | _ -> None
        in
        match named with
        | None -> Error ("cannot access field " ^ field ^ " on " ^ string_of_typ base)
        | Some (name, subst) ->
            (match SMap.find_opt name structs with
             | Some fields -> (match SMap.find_opt field fields with Some ty -> Ok ty | None -> Error ("unknown field " ^ name ^ "." ^ field))
             | None ->
                 (match SMap.find_opt name generic_structs with
                  | Some (_, fields) -> (match SMap.find_opt field fields with Some ty -> Ok (substitute_typ subst ty) | None -> Error ("unknown field " ^ name ^ "." ^ field))
                  | None -> Error (name ^ " is not a struct")))
  in
  let rec expr env = function
    | Int _ -> Ok TInt
    | Float _ -> Ok TDouble
    | Bool _ -> Ok TBool
    | Char _ -> Ok TChar
    | Null -> Ok (TPtr TVoid)
    | String _ -> Ok TString
    | Var x ->
        if SSet.mem x env.moved then Error ("use after ownership move: " ^ x)
        else (match SMap.find_opt x env.vars with
         | Some t -> Ok t
         | None ->
             (match SMap.find_opt x env.enum_values with
              | Some t -> Ok t
              | None -> Error ("unknown variable " ^ x)))
    | AddressOf (Var f) when SMap.mem f env.funcs ->
        let (ps, r) = SMap.find f env.funcs in
        Ok (TFunPtr (ps, r))
    | AddressOf e -> Result.map (fun t -> TPtr t) (expr env e)
    | Deref e ->
        Result.bind (expr env e) (function
          | TPtr t -> Ok t
          | t -> Error ("cannot dereference " ^ string_of_typ t))
    | Index (a, i) ->
        Result.bind (expr env i) (fun ti ->
                        if not (is_integer_like ti) then Error "array index must be an integer type"

          else
            Result.bind (expr env a) (function
              | TArray (t, n) ->
                  (match const_int i with
                   | Some k when k < 0 || k >= n ->
                       Error ("array index " ^ string_of_int k ^ " out of bounds for length " ^ string_of_int n)
                   | _ -> Ok t)
              | TPtr t | TDynArray t -> Ok t
               | TString -> Ok TChar
              | t -> Error ("cannot index " ^ string_of_typ t)))
    | Field (e, field) ->
        Result.bind (expr env e) (fun base -> field_type base field)
    | Call (name, [count; zero]) when name = "memory_alloc" ->
        Result.bind (expr env count) (fun tc ->
          if not (is_integer_like tc) then Error "memory_alloc count must be an integer"
          else Result.bind (expr env zero) (fun tz ->
            match tz with
            | TVoid -> Error "memory_alloc element type cannot be void"
            | _ -> Ok (TPtr tz)))
    | Call (name, [ptr; old_count; new_count; zero]) when name = "memory_resize" ->
        Result.bind (expr env ptr) (fun tp ->
          match tp with
          | TPtr elem ->
              Result.bind (expr env old_count) (fun told ->
                if not (is_integer_like told) then Error "memory_resize old count must be an integer"
                else Result.bind (expr env new_count) (fun tnew ->
                  if not (is_integer_like tnew) then Error "memory_resize new count must be an integer"
                  else Result.bind (expr env zero) (fun tz ->
                    if compatible_typ elem tz then Ok (TPtr elem)
                    else Error "memory_resize witness type mismatch")))
          | other -> Error ("memory_resize expects a pointer, got " ^ string_of_typ other))
    | Call (name, [ptr]) when name = "memory_free" ->
        Result.bind (expr env ptr) (fun tp ->
          match tp with
          | TPtr _ -> Ok TVoid
          | other -> Error ("memory_free expects a pointer, got " ^ string_of_typ other))
    | Call (name, args) ->
        (match SMap.find_opt name generic_functions with
         | Some (type_params, (formal_params, formal_return)) ->
             if List.length formal_params <> List.length args then Error ("wrong argument count for generic function " ^ name)
             else
               let rec infer subst ps actuals =
                 match ps, actuals with
                 | [], [] -> Ok subst
                 | p :: ps, a :: rest ->
                     Result.bind (expr env a) (fun actual ->
                       Result.bind (unify_typ subst p actual) (fun subst' -> infer subst' ps rest))
                 | _ -> Error "internal generic argument mismatch"
               in
               Result.bind (infer SMap.empty formal_params args) (fun subst ->
                 match List.for_all (fun p -> SMap.mem p subst) type_params with
                 | false -> Error ("cannot infer all type arguments for " ^ name)
                 | true -> Ok (substitute_typ subst formal_return))
         | None ->
        (match SMap.find_opt name env.funcs with
         | None -> (match SMap.find_opt name env.vars with
             | Some (TFunPtr (ps, r)) ->
                 if List.length ps <> List.length args then Error ("wrong argument count for " ^ name)
                 else
                   let rec args_ok ps args =
                     match ps, args with
                     | [], [] -> Ok r
                     | p :: ps, a :: rest ->
                         Result.bind (expr env a) (fun ta ->
                           if compatible_typ p ta then args_ok ps rest
                           else Error ("argument type mismatch in " ^ name))
                     | _ -> Error "internal argument mismatch"
                   in args_ok ps args
             | Some _ -> Error ("cannot call non-function value " ^ name)
             | None -> Error ("unknown function " ^ name))
         | Some (ps, r) ->
             if List.length ps <> List.length args then Error ("wrong argument count for " ^ name)
             else
               let rec args_ok ps args =
                 match ps, args with
                 | [], [] -> Ok r
                 | p :: ps, a :: rest ->
                     Result.bind (expr env a) (fun ta ->
                       if compatible_typ p ta then args_ok ps rest
                       else Error ("argument type mismatch in " ^ name))
                 | _ -> Error "internal argument mismatch"
               in
               args_ok ps args))
    | IndirectCall (callee, args) ->
        Result.bind (expr env callee) (function
          | TFunPtr (ps, r) ->
              if List.length ps <> List.length args then Error "wrong argument count for indirect call"
              else
                let rec args_ok ps args =
                  match ps, args with
                  | [], [] -> Ok r
                  | p :: ps, a :: rest ->
                      Result.bind (expr env a) (fun ta ->
                        if compatible_typ p ta then args_ok ps rest
                        else Error "argument type mismatch in indirect call")
                  | _ -> Error "internal indirect-call mismatch"
                in
                args_ok ps args
          | t -> Error ("cannot call " ^ string_of_typ t))
    | Binop (op, a, b) ->
        Result.bind (expr env a) (fun ta ->
          Result.bind (expr env b) (fun tb ->
            match op with
            | Add ->
                (match ta, tb with
                 | TPtr t, x when is_integer_like x && t <> TVoid -> Ok (TPtr t)
                 | x, TPtr t when is_integer_like x && t <> TVoid -> Ok (TPtr t)
                 | _ when is_numeric ta && is_numeric tb -> Ok (numeric_result_type ta tb)
                 | _ -> Error "addition requires numeric operands or pointer plus integer")
            | Sub ->
                (match ta, tb with
                 | TPtr t, x when is_integer_like x && t <> TVoid -> Ok (TPtr t)
                 | TPtr t, TPtr u when equal_typ t u && t <> TVoid -> Ok TInt
                 | _ when is_numeric ta && is_numeric tb -> Ok (numeric_result_type ta tb)
                 | _ -> Error "subtraction requires numeric operands, pointer minus integer, or compatible pointers")
            | Mul | Div | Mod ->
                if is_numeric ta && is_numeric tb then Ok (numeric_result_type ta tb)
                else Error "multiplication, division, and modulo require numeric operands"
            | BitAnd | BitOr | BitXor | Shl | Shr ->
                if is_integer_like ta && is_integer_like tb then Ok (integer_result_type ta tb)
                else Error "bitwise operands must be integer types"
            | And | Or ->
                if is_scalar ta && is_scalar tb then Ok TBool
                else Error "logical operands must be scalar types"
            | Eq | Neq ->
                if (is_scalar ta && is_scalar tb && compatible_typ ta tb) ||
                   (match a, b with
                    | _, (Int 0 | Null) when (match ta with TPtr _ -> true | _ -> false) -> true
                    | (Int 0 | Null), _ when (match tb with TPtr _ -> true | _ -> false) -> true
                    | _ -> false) then Ok TBool
                else Error "comparison operands are not compatible"
            | Lt | Gt ->
                if (is_numeric ta && is_numeric tb) || (match ta, tb with TPtr x, TPtr y -> equal_typ x y && x <> TVoid | _ -> false) then Ok TBool
                else Error "ordering operands must be numeric or compatible pointers"
            | Concat ->
                if equal_typ ta TString && equal_typ tb TString then Ok TString
                else Error "++ operands must be string"))
  in
  let ownership_of_initializer = function
    | Call ("alloc_ints", _) | Call ("open_file", _) | Call ("basalt_include_open_root", _) | Call ("basalt_include_open_line", _) -> true
    | _ -> false
  in
  let is_owner_type = function
    | TDynArray _ -> true
    | _ -> false
  in
  let borrow_count env x = match SMap.find_opt x env.borrow_counts with Some n -> n | None -> 0 in
  let is_borrowed env x = borrow_count env x > 0 in
  let record_borrow env destination source =
    let n = borrow_count env source in
    { env with
      borrow_counts = SMap.add source (n + 1) env.borrow_counts;
      borrow_sources = SMap.add destination source env.borrow_sources }
  in
  let release_borrow env destination =
    match SMap.find_opt destination env.borrow_sources with
    | None -> env
    | Some source ->
        let n = borrow_count env source in
        let borrow_counts = if n <= 1 then SMap.remove source env.borrow_counts else SMap.add source (n - 1) env.borrow_counts in
        { env with borrow_counts; borrow_sources = SMap.remove destination env.borrow_sources }
  in
  let move_owner env x =
    if SSet.mem x env.moved then Error ("use after ownership move: " ^ x)
    else if is_borrowed env x then Error ("cannot move borrowed value: " ^ x)
    else if not (SSet.mem x env.owned) then Error ("move requires an owned value: " ^ x)
    else Ok { env with owned = SSet.remove x env.owned; moved = SSet.add x env.moved }
  in
  let consume_call env = function
    | Call (name, args) ->
        (match SMap.find_opt name env.funcs with
         | Some (formals, _) when List.length formals = List.length args ->
             List.fold_left2
               (fun acc formal actual ->
                 Result.bind acc (fun current ->
                   if is_owner_type formal then
                     match actual with
                     | Var x -> move_owner current x
                     | _ -> Ok current
                   else Ok current))
               (Ok env) formals args
         | _ -> Ok env)
    | _ -> Ok env
  in
  let ownership_effect env = function
    | Call ("free_ints", [Var x]) | Call ("close_file", [Var x]) ->
        if SSet.mem x env.moved then Error ("double release of moved value " ^ x)
        else if is_borrowed env x then Error ("cannot release borrowed value: " ^ x)
        else if not (SSet.mem x env.owned) then Error ("release requires an owned value: " ^ x)
        else Ok { env with owned = SSet.remove x env.owned; moved = SSet.add x env.moved }
    | _ -> Ok env
  in
  let rec stmt env expected = function
    | Let (x, t, e) ->
        if SSet.mem x env.scope_vars then Error ("duplicate variable " ^ x)
        else
          Result.bind (check_type t) (fun () ->
            Result.bind (expr env e) (fun te ->
if compatible_typ t te || (match t, e with TNamed _, Int 0 | TGeneric _, Int 0 | TArray _, Int 0 | TPtr _, Null | TPtr _, Int 0 -> true | _ -> false) then
                  let move_result =
                    Ok env
                  in
                Result.bind move_result (fun moved_env ->
                  let owned = if ownership_of_initializer e then SSet.add x moved_env.owned else moved_env.owned in
                  let borrowed_env =
                    match e with
                    | AddressOf (Var source) -> record_borrow moved_env x source
                    | _ -> moved_env
                  in
                  Ok { borrowed_env with vars = SMap.add x t borrowed_env.vars; scope_vars = SSet.add x borrowed_env.scope_vars; owned; moved = SSet.remove x borrowed_env.moved })
              else
                Error ("initializer type mismatch for " ^ x ^ ": expected " ^ string_of_typ t ^ " got " ^ string_of_typ te)))
    | Const (x, t, e) ->
        if SSet.mem x env.scope_vars then Error ("duplicate variable " ^ x)
        else Result.bind (check_type t) (fun () ->
          Result.bind (expr env e) (fun te ->
            if compatible_typ t te || (match t, e with TPtr _, Null | TPtr _, Int 0 -> true | _ -> false) then
              let owned = if ownership_of_initializer e then SSet.add x env.owned else env.owned in
              Ok { env with vars = SMap.add x t env.vars; consts = SMap.add x () env.consts; scope_vars = SSet.add x env.scope_vars; owned; moved = SSet.remove x env.moved }
            else Error ("const initializer type mismatch for " ^ x)))
    | Assign (l, r) ->
        (match l with
         | Var x when SMap.mem x env.consts -> Error ("cannot assign to const " ^ x)
         | Var x when is_borrowed env x -> Error ("cannot mutate borrowed value: " ^ x)
         | _ ->
        Result.bind (expr env l) (fun tl ->
          Result.bind (expr env r) (fun tr ->
            if compatible_typ tl tr || (match l, r with _ , (Null | Int 0) -> (match tl with TPtr _ -> true | _ -> false) | _ -> false) then
              (match l, r with
               | Var x, init when ownership_of_initializer init -> Ok { env with owned = SSet.add x env.owned; moved = SSet.remove x env.moved }
               | Var x, Call ("grow_ints", [Var y; _; _]) when x = y && SSet.mem y env.owned -> Ok env
               | _ -> Ok env)
            else Error ("assignment type mismatch: " ^ string_of_typ tl ^ " <- " ^ string_of_typ tr)))
        )
    | Print e -> Result.map (fun _ -> env) (expr env e)
    | ExprStmt e ->
        Result.bind (expr env e) (fun _ ->
          Result.bind (consume_call env e) (fun moved_env -> ownership_effect moved_env e))
    | Break | Continue -> Ok env
    | Return None ->
        if equal_typ expected TVoid then Ok env
        else Error "non-void function must return a value"
    | Return (Some e) ->
        let transfer =
          match e, expected with
          | AddressOf (Var x), TPtr _ -> Error ("borrow escapes function through return: " ^ x)
          | Var x, TDynArray _ -> move_owner env x
          | _ -> Ok env
        in
        Result.bind transfer (fun return_env ->
          Result.bind (expr env e) (fun t ->
            if compatible_typ t expected || (match e, expected with Null, TPtr _ -> true | _ -> false) then Ok return_env
            else Error ("return type mismatch: expected " ^ string_of_typ expected ^ " got " ^ string_of_typ t)))
    | IfStmt (c, a, b) ->
        Result.bind (expr env c) (fun t ->
          if not (is_scalar t) then Error "if condition must be scalar"
          else Result.bind (stmt env expected a) (fun _ -> Result.map (fun _ -> env) (stmt env expected b)))
    | While (c, body) ->
        Result.bind (expr env c) (fun t ->
                      if not (is_scalar t) then Error "while condition must be scalar"

          else Result.map (fun _ -> env) (stmt env expected body))
    | For (init, c, step, body) ->
        let init_result = match init with None -> Ok env | Some s -> stmt env expected s in
        Result.bind init_result (fun env1 ->
          Result.bind (expr env1 c) (fun t ->
            if not (is_scalar t) then Error "for condition must be scalar"
            else
              Result.bind (stmt env1 expected body) (fun _ ->
                match step with
                | None -> Ok env1
                | Some s -> Result.map (fun _ -> env1) (stmt env1 expected s))))
    | Block ss ->
        let entry = env in
        let inner = { env with scope_vars = SSet.empty } in
        Result.map (fun after ->
          let unwound =
            SMap.fold
              (fun destination _ current ->
                if SMap.mem destination entry.vars then current else release_borrow current destination)
              after.borrow_sources after
          in
          { entry with
            owned = SSet.filter (fun x -> SMap.mem x entry.vars) unwound.owned;
            moved = SSet.filter (fun x -> SMap.mem x entry.vars) unwound.moved;
            borrow_counts = SMap.filter (fun source _ -> SMap.mem source entry.vars) unwound.borrow_counts;
            borrow_sources = SMap.filter (fun destination _ -> SMap.mem destination entry.vars) unwound.borrow_sources })
          (List.fold_left (fun acc s -> Result.bind acc (fun e -> stmt e expected s)) (Ok inner) ss)
  in
  let check_extern f =
    let check_ffi_type ty =
      let rec go = function
        | TInt | TBool | TChar | TString | TFloat | TDouble | TLong | TLongLong | TVoid | TParam _ -> Ok ()
        | TPtr t -> go t
        | TArray (t, _) | TDynArray t -> go t
        | TGeneric (_, args) ->
            List.fold_left (fun acc t -> Result.bind acc (fun () -> go t)) (Ok ()) args
        | TFunPtr (args, ret) ->
            List.fold_left (fun acc t -> Result.bind acc (fun () -> go t)) (Ok ()) (ret :: args)
        | TNamed name ->
            if named_type_exists name then Ok ()
            else Error ("unknown FFI type " ^ name)
      in
      go ty
    in
    let all = f.return_type :: List.map snd f.params in
    match List.find_map (fun ty -> match check_ffi_type ty with Ok () -> None | Error e -> Some e) all with
    | Some e -> Error ("invalid extern " ^ f.name ^ ": " ^ e)
    | None -> Ok ()
  in
  let globals =
    List.fold_left (fun m (x, t, _) -> SMap.add x t m) SMap.empty (program.globals @ program.consts)
  in
  let global_consts = List.fold_left (fun m (x, _, _) -> SMap.add x () m) SMap.empty program.consts in
  let base_env = { vars = globals; funcs; structs; enums; enum_values; consts = global_consts; scope_vars = SSet.empty; owned = SSet.empty; moved = SSet.empty; borrow_counts = SMap.empty; borrow_sources = SMap.empty } in
  let check_global (x, t, e) =
    Result.bind (check_type t) (fun () ->
      Result.bind (expr base_env e) (fun te ->
        if compatible_typ t te || (match t, e with TNamed _, Int 0 | TGeneric _, Int 0 | TArray _, Int 0 | TPtr _, Null | TPtr _, Int 0 -> true | _ -> false) then Ok ()
        else Error ("initializer type mismatch for " ^ x)))
  in
  let semantic_error =
    match check_struct_cycles () with
    | Error e -> Some e
    | Ok () ->
        (match List.find_map (fun f -> match check_extern f with Ok () -> None | Error e -> Some e) program.externs with
        | Some e -> Some e
        | None ->
        (match List.find_map (fun g -> match check_global g with Ok () -> None | Error e -> Some e) (program.globals @ program.consts) with
        | Some e -> Some e
        | None ->
            List.find_map
              (fun f ->
                let vars = List.fold_left (fun m (x, t) -> SMap.add x t m) globals f.params in
                let scope_vars = List.fold_left (fun s (x, _) -> SSet.add x s) SSet.empty f.params in
                (* Dynamic-array parameters are moved into the callee and become owned there. *)
                let owned = List.fold_left (fun set (name, ty) -> match ty with TDynArray _ -> SSet.add name set | _ -> set) SSet.empty f.params in
                let env = { vars; funcs; structs; enums; enum_values; consts = global_consts; scope_vars; owned; moved = SSet.empty; borrow_counts = SMap.empty; borrow_sources = SMap.empty } in
                match stmt env f.return_type f.body with Ok _ -> None | Error e -> Some e)
              program.functions))
  in
  match declaration_error with Some e -> Some e | None -> semantic_error
