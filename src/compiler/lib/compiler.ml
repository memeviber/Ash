open Ast

module SMap = Map.Make (String)

type generic_struct_info = string list * (string * typ) list
type generic_func_info = string list * func_def

type emit_env = {
  vars : typ SMap.t;
  funcs : (typ list * typ) SMap.t;
  fields : (typ SMap.t) SMap.t;
  enum_values : typ SMap.t;
  generic_structs : generic_struct_info SMap.t;
  generic_functions : generic_func_info SMap.t;
}

let empty_emit_env = {
  vars = SMap.empty; funcs = SMap.empty; fields = SMap.empty;
  enum_values = SMap.empty;
  generic_structs = SMap.empty; generic_functions = SMap.empty
}

let rec generic_type_name = function
  | TInt -> "int"
  | TBool -> "int"
  | TChar -> "char"
  | TString -> "char_ptr"
  | TFloat -> "float"
  | TDouble -> "double"
  | TVoid -> "void"
  | TPtr t -> generic_type_name t ^ "_ptr"
  | TArray (t, n) -> generic_type_name t ^ "_array_" ^ string_of_int n
  | TDynArray t -> "array_" ^ generic_type_name t
  | TFunPtr (args, r) ->
      "fn_" ^ String.concat "_" (List.map generic_type_name args) ^ "_to_" ^ generic_type_name r
  | TNamed n -> c_symbol_name n
  | TParam n -> n
  | TGeneric (n, args) -> c_symbol_name n ^ "__" ^ String.concat "__" (List.map generic_type_name args)

let rec emit_equal_typ a b =
  match a, b with
  | TInt, TInt | TBool, TBool | TInt, TBool | TBool, TInt
  | TChar, TChar | TString, TString | TFloat, TFloat
  | TDouble, TDouble | TFloat, TDouble | TDouble, TFloat
  | TVoid, TVoid -> true
  | TPtr x, TPtr y -> emit_equal_typ x y
  | TFunPtr (ap, ar), TFunPtr (bp, br) ->
      List.length ap = List.length bp && List.for_all2 emit_equal_typ ap bp && emit_equal_typ ar br
  | TDynArray x, TDynArray y -> emit_equal_typ x y
  | TNamed x, TNamed y -> String.equal x y
  | TParam x, TParam y -> String.equal x y
  | TGeneric (na, aa), TGeneric (nb, bb) ->
      String.equal na nb && List.length aa = List.length bb && List.for_all2 emit_equal_typ aa bb
  | _ -> false

let rec substitute_typ subst = function
  | TParam n -> (match SMap.find_opt n subst with Some t -> t | None -> TParam n)
  | TPtr t -> TPtr (substitute_typ subst t)
  | TArray (t, n) -> TArray (substitute_typ subst t, n)
  | TDynArray t -> TDynArray (substitute_typ subst t)
  | TFunPtr (args, r) -> TFunPtr (List.map (substitute_typ subst) args, substitute_typ subst r)
  | TGeneric (n, args) -> TGeneric (n, List.map (substitute_typ subst) args)
  | t -> t

let rec unify_generic formal actual params subst =
  match formal, actual with
  | TParam p, t when List.mem p params ->
      (match t with
       | TParam q when String.equal p q -> subst
       | _ -> (match SMap.find_opt p subst with None -> SMap.add p t subst | Some old -> if emit_equal_typ old t then subst else subst))
  | TPtr a, TPtr b -> unify_generic a b params subst
  | TArray (a, _), TArray (b, _) -> unify_generic a b params subst
  | TDynArray a, TDynArray b -> unify_generic a b params subst
  | TGeneric (na, aa), TGeneric (nb, bb) when String.equal na nb && List.length aa = List.length bb ->
      List.fold_left2 (fun m x y -> unify_generic x y params m) subst aa bb
  | TFunPtr (aa, ar), TFunPtr (ba, br) when List.length aa = List.length ba ->
      List.fold_left2 (fun m x y -> unify_generic x y params m) (unify_generic ar br params subst) aa ba
  | _ -> subst

let infer_generic_args params formal actual =
  let subst = List.fold_left2 (fun m f a -> unify_generic f a params m) SMap.empty formal actual in
  List.map (fun p -> match SMap.find_opt p subst with Some t -> t | None -> failwith ("internal: cannot infer generic argument " ^ p)) params

let rec emit_expr_type env = function
  | Int _ -> TInt
  | Bool _ -> TBool
  | Char _ -> TChar
  | Null -> TPtr TVoid
  | Float _ -> TDouble
  | String _ -> TString
  | Var x ->
      (match SMap.find_opt x env.vars with
       | Some t -> t
       | None ->
           (match SMap.find_opt x env.funcs with
            | Some (ps, r) -> TFunPtr (ps, r)
            | None ->
                (match SMap.find_opt x env.enum_values with
                 | Some t -> t
                 | None -> failwith ("internal: unknown variable or function " ^ x))))
  | Deref e ->
      (match emit_expr_type env e with
       | TPtr t -> t
       | t -> failwith ("internal: cannot dereference " ^ generic_type_name t))
  | Index (e, _) ->
      (match emit_expr_type env e with
       | TArray (t, _) | TPtr t | TDynArray t -> t
       | TString -> TChar
       | t -> failwith ("internal: cannot index " ^ generic_type_name t))
  | AddressOf (Var f) when SMap.mem f env.funcs ->
      let (ps, r) = SMap.find f env.funcs in TFunPtr (ps, r)
  | AddressOf e -> TPtr (emit_expr_type env e)
  | Field (e, f) ->
      let lookup_generic_field n args =
        let key = c_symbol_name n ^ "__" ^ String.concat "__" (List.map generic_type_name args) in
        match SMap.find_opt key env.fields with
        | Some fs -> (match SMap.find_opt f fs with Some t -> Some t | None -> None)
        | None ->
            (match SMap.find_opt n env.generic_structs with
             | Some (params, fields) when List.length params = List.length args ->
                 let subst = List.fold_left2 (fun m p t -> SMap.add p t m) SMap.empty params args in
                 (match List.assoc_opt f fields with Some t -> Some (substitute_typ subst t) | None -> None)
             | _ -> None)
      in
      let struct_field n =
        match SMap.find_opt n env.fields with
        | Some fs -> (match SMap.find_opt f fs with Some t -> Some t | None -> None)
        | None -> None
      in
      (match emit_expr_type env e with
       | TNamed n | TPtr (TNamed n) ->
           (match struct_field n with
            | Some t -> t
            | None -> failwith ("internal: unknown field " ^ n ^ "." ^ f))
       | TGeneric (n, args) | TPtr (TGeneric (n, args)) ->
           (match lookup_generic_field n args with
            | Some t -> t
            | None -> failwith ("internal: unknown field " ^ n ^ "." ^ f))
       | t -> failwith ("internal: cannot access field " ^ f ^ " on " ^ generic_type_name t))
  | Binop (op, a, b) ->
      let ta = emit_expr_type env a and tb = emit_expr_type env b in
      let numeric t = match t with TInt | TBool | TChar | TFloat | TDouble -> true | _ -> false in
      let integer t = match t with TInt | TBool | TChar -> true | _ -> false in
      let real t = match t with TFloat | TDouble -> true | _ -> false in
      (match op with
      | Concat -> TString
      | Eq | Neq | Lt | Gt | And | Or -> TBool
      | BitAnd | BitOr | BitXor | Shl | Shr -> TInt
      | Add ->
          (match ta, tb with
           | TPtr t, x when integer x -> TPtr t
           | x, TPtr t when integer x -> TPtr t
           | _ when numeric ta && numeric tb && (real ta || real tb) -> TDouble
           | _ when numeric ta && numeric tb -> TInt
           | _ -> failwith "internal: addition of incompatible operands")
      | Sub ->
          (match ta, tb with
           | TPtr _, TPtr _ -> TInt
           | TPtr t, x when integer x -> TPtr t
           | _ when numeric ta && numeric tb && (real ta || real tb) -> TDouble
           | _ when numeric ta && numeric tb -> TInt
           | _ -> failwith "internal: subtraction of incompatible operands")
      | Mul | Div | Mod ->
          if numeric ta && numeric tb && (real ta || real tb) then TDouble
          else if numeric ta && numeric tb then TInt
          else failwith "internal: arithmetic on non-numeric operands")
     | Call ("memory_alloc", [_; zero]) -> TPtr (emit_expr_type env zero)
   | Call ("memory_resize", [ptr; _; _; _]) ->
       (match emit_expr_type env ptr with TPtr t -> TPtr t | _ -> failwith "internal: memory_resize on non-pointer")
   | Call ("memory_free", _) -> TVoid

  | Call (f, args) ->
      (match SMap.find_opt f env.generic_functions with
       | Some (params, gf) ->
           let actual = List.map (emit_expr_type env) args in
           let type_args = infer_generic_args params (List.map snd gf.params) actual in
           substitute_typ (List.fold_left2 (fun m p t -> SMap.add p t m) SMap.empty params type_args) gf.return_type
       | None ->
           (match SMap.find_opt f env.funcs with
            | Some (_, t) -> t
            | None ->
                (match SMap.find_opt f env.vars with
                 | Some (TFunPtr (_, t)) -> t
                 | _ -> failwith ("internal: unknown function " ^ f))))
  | IndirectCall (f, _) ->
      (match emit_expr_type env f with
       | TFunPtr (_, t) -> t
       | t -> failwith ("internal: cannot call " ^ generic_type_name t))

let c_escape s =
  let b = Buffer.create (String.length s + 2) in
  String.iter (function
    | '\\' -> Buffer.add_string b "\\\\"
    | '"' -> Buffer.add_string b "\\\""
    | '\n' -> Buffer.add_string b "\\n"
    | '\r' -> Buffer.add_string b "\\r"
    | '\t' -> Buffer.add_string b "\\t"
    | '\b' -> Buffer.add_string b "\\b"
    | c ->
        let n = Char.code c in
        if n < 32 || n > 126 then
          Buffer.add_string b (Printf.sprintf "\\%03o" n)
        else Buffer.add_char b c
  ) s;
  Buffer.contents b

let rec compile_typ = function
  | TInt -> "int"
  | TBool -> "int"
  | TChar -> "char"
  | TString -> "char*"
  | TFloat -> "float"
  | TDouble -> "double"
  | TVoid -> "void"
  | TPtr t -> compile_typ t ^ "*"
  | TArray (t, n) -> compile_typ t ^ "[" ^ string_of_int n ^ "]"
  | TDynArray _ -> failwith "dynamic arrays are not supported; use array::Array<T>"
  | TFunPtr (_, r) -> compile_typ r
  | TNamed n -> c_symbol_name n
  | TParam n -> n
  | TGeneric (n, args) -> c_symbol_name n ^ "__" ^ String.concat "__" (List.map generic_type_name args)

let substitute_expr _subst e = e

let rec substitute_stmt subst = function
  | Let (x, t, e) -> Let (x, substitute_typ subst t, substitute_expr subst e)
  | Const (x, t, e) -> Const (x, substitute_typ subst t, substitute_expr subst e)
  | Assign (l, r) -> Assign (substitute_expr subst l, substitute_expr subst r)
  | Print e -> Print (substitute_expr subst e)
  | IfStmt (c, a, b) -> IfStmt (substitute_expr subst c, substitute_stmt subst a, substitute_stmt subst b)
  | While (c, b) -> While (substitute_expr subst c, substitute_stmt subst b)
  | For (a, c, b, body) -> For (Option.map (substitute_stmt subst) a, substitute_expr subst c, Option.map (substitute_stmt subst) b, substitute_stmt subst body)
  | Block ss -> Block (List.map (substitute_stmt subst) ss)
  | Return eo -> Return (Option.map (substitute_expr subst) eo)
  | ExprStmt e -> ExprStmt (substitute_expr subst e)
  | Break -> Break
  | Continue -> Continue

let instantiate_func params f args name =
  let subst = List.fold_left2 (fun m p a -> SMap.add p a m) SMap.empty params args in
  { name; params = List.map (fun (x, t) -> (x, substitute_typ subst t)) f.params;
    return_type = substitute_typ subst f.return_type;
    body = substitute_stmt subst f.body }

let rec collect_typ add = function
  | TPtr t -> collect_typ add t
  | TArray (t, _) -> collect_typ add t
  | TFunPtr (args, r) -> List.iter (collect_typ add) args; collect_typ add r
  | TGeneric (n, args) -> add (TGeneric (n, args)); List.iter (collect_typ add) args
  | _ -> ()

let rec collect_expr add = function
  | Binop (_, a, b) -> collect_expr add a; collect_expr add b
  | Call (_, es) | IndirectCall (_, es) -> List.iter (collect_expr add) es
  | Deref e | AddressOf e -> collect_expr add e
  | Index (a, i) -> collect_expr add a; collect_expr add i
  | Field (e, _) -> collect_expr add e
  | _ -> ()

let rec collect_stmt add = function
  | Let (_, t, e) | Const (_, t, e) -> collect_typ add t; collect_expr add e
  | Assign (a, b) -> collect_expr add a; collect_expr add b
  | Print e | ExprStmt e -> collect_expr add e
  | IfStmt (c, a, b) -> collect_expr add c; collect_stmt add a; collect_stmt add b
  | While (c, b) -> collect_expr add c; collect_stmt add b
  | For (a, c, b, body) -> Option.iter (collect_stmt add) a; collect_expr add c; Option.iter (collect_stmt add) b; collect_stmt add body
  | Block ss -> List.iter (collect_stmt add) ss
  | Return eo -> Option.iter (collect_expr add) eo
  | Break | Continue -> ()

let c_type_of_element t = compile_typ t

let rec compile_decl t name =
  match t with
  | TFunPtr (args, ret) ->
      let arg_names = List.mapi (fun i ty -> compile_decl ty ("a" ^ string_of_int i)) args in
      compile_typ ret ^ " (*" ^ name ^ ")(" ^ String.concat ", " arg_names ^ ")"
  | TArray (inner, n) -> compile_decl inner name ^ "[" ^ string_of_int n ^ "]"
  | _ -> compile_typ t ^ " " ^ name

let compile_ffi_typ = function
  | TString -> "const char*"
  | t -> compile_typ t

let compile_ffi_decl t name =
  match t with
  | TFunPtr (args, ret) ->
      let arg_names = List.mapi (fun i ty -> compile_ffi_typ ty ^ " a" ^ string_of_int i) args in
      compile_typ ret ^ " (*" ^ name ^ ")(" ^ String.concat ", " arg_names ^ ")"
  | _ -> compile_ffi_typ t ^ " " ^ name

let compile_extern_prototype f =
  let args = match f.params with
    | [] -> "void"
    | ps -> List.map (fun (x, t) -> compile_ffi_decl t x) ps |> String.concat ", "
  in
  compile_typ f.return_type ^ " " ^ c_symbol_name f.name ^ "(" ^ args ^ ");\n"

let c_escape_char = function
  | '\'' -> "\\'"
  | '\\' -> "\\\\"
  | '\n' -> "\\n"
  | '\r' -> "\\r"
  | '\t' -> "\\t"
  | '\b' -> "\\b"
  | '\012' -> "\\f"
  | '\011' -> "\\v"
  | '\000' -> "\\0"
  | c -> String.make 1 c

let rec compile_expr env = function
  | Int i -> string_of_int i
  | Float f ->
      let s = Printf.sprintf "%.17g" f in
      if String.contains s '.' || String.contains s 'e' || String.contains s 'E' then s else s ^ ".0"
  | Bool b -> if b then "1" else "0"
  | Char c -> "'" ^ c_escape_char c ^ "'"
  | Null -> "NULL"
  | String s -> "\"" ^ c_escape s ^ "\""
  | Var s -> c_symbol_name s
  | Binop (op, e1, e2) ->
      let sop = match op with
        | Add -> "+" | Sub -> "-" | Mul -> "*" | Div -> "/" | Mod -> "%"
        | Eq -> "==" | Neq -> "!=" | Lt -> "<" | Gt -> ">"
        | And -> "&&" | Or -> "||" | Concat -> "++"
        | BitAnd -> "&" | BitOr -> "|" | BitXor -> "^" | Shl -> "<<" | Shr -> ">>"
      in
      if op = Concat then
        "runtime_string_concat(" ^ compile_expr env e1 ^ ", " ^ compile_expr env e2 ^ ")"
      else (
        let rendered = "(" ^ compile_expr env e1 ^ " " ^ sop ^ " " ^ compile_expr env e2 ^ ")" in
        match op, emit_expr_type env e1, emit_expr_type env e2 with
        | Sub, TPtr _, TPtr _ -> "(int)" ^ rendered
        | _ -> rendered)
  | Call ("memory_alloc", [count; zero]) ->
      let et = emit_expr_type env zero in
      let cty = c_type_of_element et in
      "((void)(" ^ compile_expr env zero ^ "), (" ^ cty ^ "*)basalt_memory_alloc(" ^ compile_expr env count ^ ", sizeof(" ^ cty ^ ")))"
  | Call ("memory_resize", [ptr; old_count; new_count; zero]) ->
      let et = match emit_expr_type env ptr with TPtr t -> t | _ -> TVoid in
      let cty = c_type_of_element et in
      "((void)(" ^ compile_expr env zero ^ "), (" ^ cty ^ "*)basalt_memory_resize((void*)" ^ compile_expr env ptr ^ ", " ^ compile_expr env old_count ^ ", " ^ compile_expr env new_count ^ ", sizeof(" ^ cty ^ ")))"
  | Call ("memory_free", [ptr]) ->
      "basalt_memory_free((void*)" ^ compile_expr env ptr ^ ")"
  | Call (f, args) ->
      let rendered = List.map (compile_expr env) args |> String.concat ", " in
      let target =
        match SMap.find_opt f env.generic_functions with
        | Some (params, gf) ->
            let actual = List.map (emit_expr_type env) args in
            let type_args = infer_generic_args params (List.map snd gf.params) actual in
            c_symbol_name f ^ "__" ^ String.concat "__" (List.map generic_type_name type_args)
        | None -> c_symbol_name f
      in
      (match f, args with
              | "open_file", _ -> "(void*)open_file(" ^ rendered ^ ")"
       | "read_char", _ -> "read_char(" ^ rendered ^ ")"
       | "close_file", _ -> "(int)close_file(" ^ rendered ^ ")"
       | "write_char", _ -> "write_char(" ^ rendered ^ ")"
       | "write_string", _ -> "write_string(" ^ rendered ^ ")"
       | "write_int", _ -> "write_int(" ^ rendered ^ ")"
       | "alloc_ints", _ -> "(int*)alloc_ints(" ^ rendered ^ ")"
       | "grow_ints", _ -> "(int*)grow_ints(" ^ rendered ^ ")"
       | "free_ints", _ -> "free_ints(" ^ rendered ^ ")"
       | _ -> target ^ "(" ^ rendered ^ ")")
  | IndirectCall (f, args) ->
      "(" ^ compile_expr env f ^ ")(" ^ (List.map (compile_expr env) args |> String.concat ", ") ^ ")"
  | Deref e -> "*(" ^ compile_expr env e ^ ")"
  | Index (e, idx) -> "(" ^ compile_expr env e ^ ")[" ^ compile_expr env idx ^ "]"
  | AddressOf e -> "&( " ^ compile_expr env e ^ ")"
  | Field (e, f) ->
      let access =
        match emit_expr_type env e with
        | TPtr (TNamed _) | TPtr (TGeneric _) -> "->"
        | _ -> "."
      in
      "(" ^ compile_expr env e ^ ")" ^ access ^ f

let format_for_type = function
  | TString -> "%s"
  | TChar -> "%c"
  | TFloat | TDouble -> "%g"
  | TPtr _ -> "%p"
  | _ -> "%d"

let format_for_expr env e =
  match e with
  | Binop (Sub, a, b) ->
      (match emit_expr_type env a, emit_expr_type env b with
       | TPtr _, TPtr _ -> "%d"
       | _ -> format_for_type (emit_expr_type env e))
  | _ -> format_for_type (emit_expr_type env e)

let compile_for_clause env = function
  | Let (x, t, e) -> compile_typ t ^ " " ^ x ^ " = " ^ compile_expr env e
  | Assign (lv, rv) -> compile_expr env lv ^ " = " ^ compile_expr env rv
  | ExprStmt e -> compile_expr env e
  | _ -> ""

let compile_initializer env t e =
  match t, e with
  | TNamed n, Int 0 when SMap.mem n env.fields -> "{0}"
  | TGeneric _, Int 0 -> "{0}"
  | _ -> compile_expr env e

let rec compile_stmt env indent = function
  | Let (x, t, e) ->
             (match t with
       | TArray (inner, n) ->
           indent ^ compile_typ inner ^ " " ^ x ^ "[" ^ string_of_int n ^ "] = {0};\n"
       | _ -> indent ^ compile_decl t x ^ " = " ^ compile_initializer env t e ^ ";\n")
  | Const (x, t, e) -> indent ^ "const " ^ compile_decl t x ^ " = " ^ compile_initializer env t e ^ ";\n"

  | Assign (lv, rv) -> indent ^ compile_expr env lv ^ " = " ^ compile_expr env rv ^ ";\n"
  | Print e ->
      let fmt = format_for_expr env e in
      let arg = if fmt = "%p" then "(void*)" ^ compile_expr env e else compile_expr env e in
      indent ^ "printf(\"" ^ fmt ^ "\\n\", " ^ arg ^ ");\n"
  | IfStmt (c, t, e) ->
      indent ^ "if (" ^ compile_expr env c ^ ") {\n"
      ^ compile_stmt env (indent ^ "  ") t
      ^ indent ^ "} else {\n"
      ^ compile_stmt env (indent ^ "  ") e
      ^ indent ^ "}\n"
  | While (c, body) ->
      indent ^ "while (" ^ compile_expr env c ^ ") {\n"
      ^ compile_stmt env (indent ^ "  ") body
      ^ indent ^ "}\n"
  | For (init, c, step, body) ->
      let i = match init with Some s -> compile_for_clause env s
 | None -> "" in
      let s = match step with Some x -> compile_for_clause env x
 | None -> "" in
      indent ^ "for (" ^ i ^ "; " ^ compile_expr env c ^ "; " ^ s ^ ") {\n"
      ^ compile_stmt env (indent ^ "  ") body
      ^ indent ^ "}\n"
  | Block ss ->
      let rec compile_block env = function
        | [] -> ""
        | s :: rest ->
            let rendered = compile_stmt env (indent ^ "  ") s in
            let env' = match s with
              | Let (x, t, _) | Const (x, t, _) -> { env with vars = SMap.add x t env.vars }
              | _ -> env
            in
            rendered ^ compile_block env' rest
      in
      indent ^ "{\n" ^ compile_block env ss ^ indent ^ "}\n"
  | Return None -> indent ^ "return;\n"
  | Return (Some e) -> indent ^ "return " ^ compile_expr env e ^ ";\n"
  | ExprStmt e -> indent ^ compile_expr env e ^ ";\n"
  | Break -> indent ^ "break;\n"
  | Continue -> indent ^ "continue;\n"

let params f =
  match f.params with
  | [] -> "void"
  | ps -> List.map (fun (x, t) -> compile_decl t x) ps |> String.concat ", "

let compile_func env f =
  let return_type = if f.name = "main" then "int" else compile_typ f.return_type in
  let local_vars = List.fold_left (fun m (x, t) -> SMap.add x t m) env.vars f.params in
  let fenv = { env with vars = local_vars } in
  if f.name = "main" then
    let body =
      match f.body with
      | Block ss -> compile_stmt fenv "" (Block ss) ^ "  return 0;\n"
      | _ -> compile_stmt fenv "" f.body ^ "  return 0;\n"
    in
    return_type ^ " main(" ^ params f ^ ") {\n" ^ body ^ "}\n"
  else return_type ^ " " ^ c_symbol_name f.name ^ "(" ^ params f ^ ") " ^ compile_stmt fenv "" f.body

let compile_prototype f =
  let return_type = if f.name = "main" then "int" else compile_typ f.return_type in
  return_type ^ " " ^ (if f.name = "main" then "main" else c_symbol_name f.name)
  ^ "(" ^ params f ^ ");\n"

let compile ?(c_includes = "") program =
  let struct_instances = ref [] in
  let function_instances = ref [] in
  let seen_struct = Hashtbl.create 32 in
  let seen_function = Hashtbl.create 32 in
  let struct_key (n, args) = c_symbol_name n ^ "__" ^ String.concat "__" (List.map generic_type_name args) in
  let function_key (n, args) = c_symbol_name n ^ "__" ^ String.concat "__" (List.map generic_type_name args) in
  let add_struct t =
    match t with
    | TGeneric (n, args) ->
        let key = struct_key (n, args) in
        if not (Hashtbl.mem seen_struct key) then (Hashtbl.add seen_struct key (); struct_instances := (n, args) :: !struct_instances)
    | _ -> ()
  in
  let add_function n args =
    let key = function_key (n, args) in
    if not (Hashtbl.mem seen_function key) then (Hashtbl.add seen_function key (); function_instances := (n, args) :: !function_instances)
  in
  let rec scan_expr env e =
    collect_typ add_struct (emit_expr_type env e);
    (match e with
     | Binop (_, a, b) -> scan_expr env a; scan_expr env b
     | Call (n, args) ->
         (match SMap.find_opt n env.generic_functions with
          | Some (params, gf) ->
              let actual = List.map (emit_expr_type env) args in
              add_function n (infer_generic_args params (List.map snd gf.params) actual)
          | None -> ());
         List.iter (scan_expr env) args
     | IndirectCall (f, args) -> scan_expr env f; List.iter (scan_expr env) args
     | Deref x | AddressOf x -> scan_expr env x
     | Index (a, i) -> scan_expr env a; scan_expr env i
     | Field (x, _) -> scan_expr env x
     | Int _ | Float _ | Bool _ | Char _ | Null | String _ | Var _ -> ())
  in
  let rec scan_stmt env = function
    | Let (_, t, e) | Const (_, t, e) -> collect_typ add_struct t; scan_expr env e
    | Assign (a, b) -> scan_expr env a; scan_expr env b
    | Print e | ExprStmt e -> scan_expr env e
    | IfStmt (c, a, b) -> scan_expr env c; scan_stmt env a; scan_stmt env b
    | While (c, b) -> scan_expr env c; scan_stmt env b
    | For (a, c, b, body) -> Option.iter (scan_stmt env) a; scan_expr env c; Option.iter (scan_stmt env) b; scan_stmt env body
    | Block ss ->
        let rec loop local = function
          | [] -> ()
          | s :: rest ->
              scan_stmt local s;
              let local' = match s with Let (x, t, _) | Const (x, t, _) -> { local with vars = SMap.add x t local.vars } | _ -> local in
              loop local' rest
        in loop env ss
    | Return eo -> Option.iter (scan_expr env) eo
    | Break | Continue -> ()
  in
  let process_functions env =
    let processed = Hashtbl.create 32 in
    let rec loop () =
      let before = List.length !function_instances in
      List.iter (fun (n, args) ->
        let key = function_key (n, args) in
        if not (Hashtbl.mem processed key) then begin
          Hashtbl.add processed key ();
          match SMap.find_opt n env.generic_functions with
          | Some (params, gf) ->
              let inst = instantiate_func params gf args key in
              let local_vars = List.fold_left (fun m (x, t) -> SMap.add x t m) env.vars inst.params in
              scan_stmt { env with vars = local_vars } inst.body
          | None -> ()
        end) !function_instances;
      if List.length !function_instances <> before then loop ()
    in
    loop ()
  in
  let include_runtime = {|#if defined(_WIN32)
#include <direct.h>
#else
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#if defined(__GNUC__) || defined(__clang__)
#define BASALT_UNUSED __attribute__((unused))
#else
#define BASALT_UNUSED
#endif
static void* basalt_track(void*);
static char** basalt_inc_active=NULL;static size_t basalt_inc_active_n=0,basalt_inc_active_cap=0;static char** basalt_inc_loaded=NULL;static size_t basalt_inc_loaded_n=0,basalt_inc_loaded_cap=0;static int basalt_inc_status=0;
static BASALT_UNUSED size_t basalt_inc_find(char**v,size_t n,const char*p){size_t i;for(i=0;i<n;i++)if(strcmp(v[i],p)==0)return i;return (size_t)-1;}
static BASALT_UNUSED void basalt_inc_add(char***vp,size_t*np,size_t*cp,char*p){size_t c;char**q;if(*np==*cp){c=*cp?*cp*2:16;q=(char**)realloc(*vp,c*sizeof(char*));if(!q)exit(2);*vp=q;*cp=c;}(*vp)[(*np)++]=p;}
static BASALT_UNUSED char* basalt_inc_strdup(const char*p){size_t n=strlen(p);char*q=(char*)malloc(n+1);if(!q)exit(2);memcpy(q,p,n+1);return(char*)basalt_track(q);}
static BASALT_UNUSED char* basalt_inc_realpath(const char*p){
#if defined(_WIN32)
 char*q=_fullpath(NULL,p,0);if(q)return(char*)basalt_track(q);
#else
 char*q=realpath(p,NULL);if(q)return(char*)basalt_track(q);
#endif
 return basalt_inc_strdup(p);
}
static BASALT_UNUSED int basalt_inc_begin(char*p){if(basalt_inc_find(basalt_inc_active,basalt_inc_active_n,p)!=(size_t)-1){basalt_inc_status=1;return 0;}if(basalt_inc_find(basalt_inc_loaded,basalt_inc_loaded_n,p)!=(size_t)-1){basalt_inc_status=2;return 0;}basalt_inc_add(&basalt_inc_active,&basalt_inc_active_n,&basalt_inc_active_cap,p);basalt_inc_status=0;return 1;}
static BASALT_UNUSED void basalt_include_close(void){if(basalt_inc_active_n){char*p=basalt_inc_active[--basalt_inc_active_n];if(basalt_inc_find(basalt_inc_loaded,basalt_inc_loaded_n,p)==(size_t)-1)basalt_inc_add(&basalt_inc_loaded,&basalt_inc_loaded_n,&basalt_inc_loaded_cap,p);}}
static BASALT_UNUSED char* basalt_inc_join(const char*base,const char*raw){const char*s=strrchr(base,'/');size_t n=s?(size_t)(s-base+1):0;size_t m=strlen(raw);char*q=(char*)malloc(n+m+1);if(!q)exit(2);if(n)memcpy(q,base,n);memcpy(q+n,raw,m+1);return(char*)basalt_track(q);}
static BASALT_UNUSED int basalt_include_line_mode(int*line,int n){int i=0,j;while(i<n&&(line[i]==' '||line[i]=='\t'))i++;if(i+7<=n&&!memcmp(line+i,"include",7)){j=i+7;while(j<n&&(line[j]==' '||line[j]=='\t'))j++;if(j<n&&line[j]=='\"')return 1;}if(i+8<=n&&!memcmp(line+i,"includec",8)){j=i+8;while(j<n&&(line[j]==' '||line[j]=='\t'))j++;if(j<n&&line[j]=='\"')return 2;}return 0;}
static BASALT_UNUSED void* basalt_include_open_root(const char*path){char*p=basalt_inc_realpath(path);FILE*f;if(!basalt_inc_begin(p))return NULL;f=fopen(p,"r");if(!f){basalt_inc_status=3;basalt_include_close();return NULL;}return(void*)f;}
static BASALT_UNUSED void* basalt_include_open_line(int*line,int n,int mode){int i=0,a,b,j;char*raw,*joined,*canon;FILE*f;(void)mode;while(i<n&&line[i]!='"')i++;if(i>=n)return NULL;a=++i;while(i<n&&line[i]!='"')i++;if(i>=n)return NULL;b=i;raw=(char*)malloc((size_t)(b-a)+1);if(!raw)exit(2);{int k;for(k=0;k<b-a;k++)raw[k]=(char)line[a+k];}raw[b-a]=0;raw=(char*)basalt_track(raw);j=i+1;while(j<n&&(line[j]==' '||line[j]=='\t'))j++;if(j<n&&line[j]==';')j++;while(j<n&&(line[j]==' '||line[j]=='\t'))j++;if(j!=n)return NULL;joined=basalt_inc_join(basalt_inc_active[basalt_inc_active_n-1],raw);canon=basalt_inc_realpath(joined);if(!basalt_inc_begin(canon))return NULL;f=fopen(canon,"r");if(!f){basalt_inc_status=3;basalt_include_close();return NULL;}return(void*)f;}
static BASALT_UNUSED int basalt_include_last_status(void){return basalt_inc_status;}
static BASALT_UNUSED void basalt_include_reset_session(void){basalt_inc_active_n=0;basalt_inc_loaded_n=0;basalt_inc_status=0;}
|} in
  let header =
    include_runtime ^
    (if c_includes = "" then "" else c_includes ^ "\n")
    ^ "static BASALT_UNUSED void* open_file(const char* path, const char* mode) {\n"
    ^ "  return (void*)fopen(path, mode);\n}\n"
    ^ "static BASALT_UNUSED int read_char(void* handle) {\n"
    ^ "  int c = fgetc((FILE*)handle);\n"
    ^ "  return c == EOF ? -1 : c;\n}\n"
    ^ "static BASALT_UNUSED int close_file(void* handle) {\n"
    ^ "  return fclose((FILE*)handle);\n}\n"
    ^ "static BASALT_UNUSED int write_char(void* handle, int c) {\n"
    ^ "  return fputc(c, (FILE*)handle);\n}\n"
    ^ "static BASALT_UNUSED int write_string(void* handle, const char* s) {\n"
    ^ "  return fputs(s, (FILE*)handle);\n}\n"
    ^ "static void** basalt_live = NULL; static size_t basalt_live_n = 0, basalt_live_cap = 0;\n"
    ^ "static BASALT_UNUSED void basalt_panic(int code) { fprintf(stderr, \"Basalt memory error %d\\n\", code); exit(2); }\n"
    ^ "static BASALT_UNUSED size_t basalt_checked_bytes(int count, size_t elem_size) { if (count < 0) basalt_panic(1); if (elem_size != 0 && (size_t)count > (size_t)-1 / elem_size) basalt_panic(1); return (size_t)count * elem_size; }\n"
    ^ "static BASALT_UNUSED size_t basalt_find(void* p) { size_t i; for (i = 0; i < basalt_live_n; ++i) if (basalt_live[i] == p) return i; return (size_t)-1; }\n"
    ^ "static BASALT_UNUSED void basalt_validate(void) { size_t i, j; for (i = 0; i < basalt_live_n; ++i) { if (!basalt_live[i]) basalt_panic(2); for (j = i + 1; j < basalt_live_n; ++j) if (basalt_live[i] == basalt_live[j]) basalt_panic(2); } }\n"
    ^ "static BASALT_UNUSED void basalt_cleanup(void) { size_t i; basalt_validate(); for (i = 0; i < basalt_live_n; ++i) free(basalt_live[i]); free(basalt_live); basalt_live = NULL; basalt_live_n = basalt_live_cap = 0; }\n"
    ^ "static BASALT_UNUSED void* basalt_track(void* p) { size_t c; void** q; if (!p) return NULL; if (basalt_find(p) != (size_t)-1) basalt_panic(2); if (basalt_live_n == basalt_live_cap) { if (basalt_live_cap > (size_t)-1 / 2) basalt_panic(2); c = basalt_live_cap ? basalt_live_cap * 2 : 32; if (c > (size_t)-1 / sizeof(void*)) basalt_panic(2); q = (void**)realloc(basalt_live, c * sizeof(void*)); if (!q) basalt_panic(2); basalt_live = q; basalt_live_cap = c; } basalt_live[basalt_live_n++] = p; atexit(basalt_cleanup); return p; }\n"
    ^ "static BASALT_UNUSED void basalt_release(void* p) { size_t i; if (!p) return; i = basalt_find(p); if (i == (size_t)-1) basalt_panic(2); free(p); basalt_live[i] = basalt_live[--basalt_live_n]; }\n"
    ^ "static BASALT_UNUSED void* basalt_memory_alloc(int count, size_t elem_size) { size_t bytes = basalt_checked_bytes(count, elem_size); void* p = calloc(1, bytes ? bytes : 1); if (!p) basalt_panic(5); return basalt_track(p); }\n"
    ^ "static BASALT_UNUSED void* basalt_memory_resize(void* old, int old_count, int new_count, size_t elem_size) { size_t slot = (size_t)-1; size_t old_bytes; size_t new_bytes; void* p; if (old_count < 0 || new_count < 0) basalt_panic(1); if (new_count < old_count) basalt_panic(1); if (old) { slot = basalt_find(old); if (slot == (size_t)-1) basalt_panic(2); } old_bytes = basalt_checked_bytes(old_count, elem_size); new_bytes = basalt_checked_bytes(new_count, elem_size); p = realloc(old, new_bytes ? new_bytes : 1); if (!p) basalt_panic(6); if (slot == (size_t)-1) basalt_track(p); else basalt_live[slot] = p; if (new_bytes > old_bytes) memset((char*)p + old_bytes, 0, new_bytes - old_bytes); return p; }\n"
    ^ "static BASALT_UNUSED void basalt_memory_free(void* p) { basalt_release(p); }\n"
    ^ "static BASALT_UNUSED char* runtime_string_concat(const char* a, const char* b) {\n"
    ^ "  size_t na, nb, total; char* p; if (!a || !b) basalt_panic(4); na = strlen(a); nb = strlen(b); if (na > (size_t)-1 - nb - 1) basalt_panic(1); total = na + nb + 1; p = (char*)malloc(total); if (!p) basalt_panic(5); memcpy(p, a, na); memcpy(p + na, b, nb); p[na + nb] = 0; return (char*)basalt_track(p);\n}\n"
    ^ "static BASALT_UNUSED int write_int(int* handle, int value) {\n"
    ^ "  return fprintf((FILE*)handle, \"%d\", value);\n}\n"
    ^ "static BASALT_UNUSED int* alloc_ints(int count) { int* p; if (count < 0) basalt_panic(1); if (count < 1) count = 1; basalt_checked_bytes(count, sizeof(int)); p = (int*)calloc((size_t)count, sizeof(int)); if (!p) basalt_panic(5); return (int*)basalt_track(p); }\n"
    ^ "static BASALT_UNUSED void free_ints(int* p) { basalt_release(p); }\n"
    ^ "static BASALT_UNUSED int* grow_ints(int* old, int old_count, int new_count) { size_t slot = (size_t)-1; int* p; if (old_count < 0 || new_count < 0) basalt_panic(1); if (new_count <= old_count) return old; if (old) { slot = basalt_find(old); if (slot == (size_t)-1) basalt_panic(2); } basalt_checked_bytes(new_count, sizeof(int)); p = (int*)realloc(old, (size_t)new_count * sizeof(int)); if (!p) basalt_panic(6); if (old) basalt_live[slot] = p; else basalt_track(p); memset(p + old_count, 0, (size_t)(new_count - old_count) * sizeof(int)); return p; }\n\n"
 in
  let emit_env =
    let funcs =
      List.fold_left (fun m f -> SMap.add f.name (List.map snd f.params, f.return_type) m)
        (List.fold_left (fun m (name, sig_) -> SMap.add name sig_ m)
           (List.fold_left (fun m f -> SMap.add f.name (List.map snd f.params, f.return_type) m) SMap.empty program.externs)
           Ast.builtin_funcs)
        program.functions
    in
    let fields = List.fold_left (fun m (n, fs) -> SMap.add n (List.fold_left (fun fm (x, t) -> SMap.add x t fm) SMap.empty fs) m) SMap.empty program.structs in
    let enum_values =
      List.fold_left
        (fun m (name, values) ->
          List.fold_left (fun vm value -> SMap.add value (TNamed name) vm) m values)
        SMap.empty program.enums
    in
    let vars = List.fold_left (fun m (x, t, _) -> SMap.add x t m) SMap.empty (program.globals @ program.consts) in
    let generic_structs =
      List.fold_left (fun m (n, ps, fs) -> SMap.add n (ps, fs) m) SMap.empty program.generic_structs
    in
    let generic_functions =
      List.fold_left (fun m (n, ps, f) -> SMap.add n (ps, f) m) SMap.empty program.generic_functions
    in
    { vars; funcs; fields; enum_values; generic_structs; generic_functions }
  in
  List.iter (fun (_, t, e) -> collect_typ add_struct t; scan_expr emit_env e) (program.globals @ program.consts);
  List.iter (fun f ->
    let local_vars = List.fold_left (fun m (x, t) -> SMap.add x t m) emit_env.vars f.params in
    scan_stmt { emit_env with vars = local_vars } f.body) program.functions;
  process_functions emit_env;
  let specialized_fields =
    List.fold_left (fun m (n, args) ->
      match SMap.find_opt n emit_env.generic_structs with
      | Some (params, fs) ->
          let subst = List.fold_left2 (fun s p t -> SMap.add p t s) SMap.empty params args in
          let concrete = List.map (fun (x, t) -> (x, substitute_typ subst t)) fs in
          SMap.add (struct_key (n, args)) (List.fold_left (fun fm (x, t) -> SMap.add x t fm) SMap.empty concrete) m
      | None -> m) emit_env.fields !struct_instances
  in
  let emit_env = { emit_env with fields = specialized_fields } in
  let specialized_functions =
    List.filter_map (fun (n, args) ->
      match SMap.find_opt n emit_env.generic_functions with
      | Some (params, gf) -> Some (instantiate_func params gf args (function_key (n, args)))
      | None -> None) (List.rev !function_instances)
  in
  let type_decls =
    let specialized = List.rev !struct_instances in
    let struct_forward =
      (List.map (fun (name, _) -> "typedef struct " ^ c_symbol_name name ^ " " ^ c_symbol_name name ^ ";\n") program.structs @
       List.map (fun (name, args) -> "typedef struct " ^ struct_key (name, args) ^ " " ^ struct_key (name, args) ^ ";\n") specialized)
      |> String.concat ""
    in
    let enum_forward =
      ""
    in
    let struct_defs =
      let ordinary = List.map (fun (name, fields) ->
        "struct " ^ c_symbol_name name ^ " {\n" ^
        (List.map (fun (x, t) -> "  " ^ compile_decl t x ^ ";\n") fields |> String.concat "") ^
        "};\n") program.structs in
      let concrete = List.filter_map (fun (name, args) ->
        match SMap.find_opt name emit_env.generic_structs with
        | None -> None
        | Some (params, fields) ->
            let subst = List.fold_left2 (fun s p t -> SMap.add p t s) SMap.empty params args in
            let cname = struct_key (name, args) in
            let fs = List.map (fun (x, t) -> (x, substitute_typ subst t)) fields in
            Some ("struct " ^ cname ^ " {\n" ^
              (List.map (fun (x, t) -> "  " ^ compile_decl t x ^ ";\n") fs |> String.concat "") ^
              "};\n")) (List.rev !struct_instances) in
      String.concat "" (ordinary @ concrete)
    in
    let enum_defs =
      List.map (fun (name, values) ->
        "enum " ^ c_symbol_name name ^ " { " ^ String.concat ", " values ^ " };\n" ^
        "typedef enum " ^ c_symbol_name name ^ " " ^ c_symbol_name name ^ ";\n") program.enums
      |> String.concat ""
    in
    struct_forward ^ enum_forward ^ struct_defs ^ enum_defs
  in
  let globals =
    List.map (fun (x, t, e) ->
      match t, e with
      | TArray (inner, n), _ ->
          compile_typ inner ^ " " ^ c_symbol_name x ^ "[" ^ string_of_int n ^ "] = {0};\n"
       | (TNamed _ | TGeneric _), Int 0 -> compile_decl t (c_symbol_name x) ^ " = {0};\n"



       | _ -> compile_decl t (c_symbol_name x) ^ " = " ^ compile_expr emit_env e ^ ";\n"
    ) (program.globals @ program.consts) |> String.concat "" in
  let extern_prototypes =
    List.map compile_extern_prototype program.externs |> String.concat "" in
  let prototypes =
    (List.map compile_prototype program.functions @ List.map compile_prototype specialized_functions) |> String.concat "" in
  let functions =
    (List.map (compile_func emit_env) program.functions @ List.map (compile_func emit_env) specialized_functions) |> String.concat "\n" in
  let fallback =
    if List.exists (fun f -> f.name = "main") program.functions then ""
    else "int main(void) { return 0; }\n" in
  header ^ type_decls ^ globals ^ "\n" ^ extern_prototypes ^ prototypes ^ "\n" ^ functions ^ fallback
