type typ = TInt | TBool | TChar | TString | TFloat | TDouble | TVoid | TPtr of typ | TArray of typ * int | TDynArray of typ | TNamed of string | TParam of string | TGeneric of string * typ list | TFunPtr of typ list * typ

type binop = Add | Sub | Mul | Div | Mod | Eq | Neq | Lt | Gt | And | Or | Concat | BitAnd | BitOr | BitXor | Shl | Shr

type expr =
  | Int of int
  | Bool of bool
  | Char of char
  | Null
  | Float of float
  | String of string
  | Var of string
  | Binop of binop * expr * expr
  | Call of string * expr list
  | IndirectCall of expr * expr list
  | Deref of expr
  | Index of expr * expr
  | AddressOf of expr
  | Field of expr * string

type stmt =
  | Let of string * typ * expr
  | Const of string * typ * expr
  | Assign of expr * expr (* lvalue = rvalue *)
  | Print of expr
  | IfStmt of expr * stmt * stmt
  | While of expr * stmt
  | For of stmt option * expr * stmt option * stmt
  | Block of stmt list
  | Return of expr option
  | ExprStmt of expr
  | Break
  | Continue

type param = string * typ

type func_def = {
  name : string;
  params : param list;
  return_type : typ;
  body : stmt;
}

type global_def = string * typ * expr

type decl =
  | GDecl of global_def
  | GConst of global_def
  | FDecl of func_def
  | StructDecl of string * (string * typ) list
  | GenericStructDecl of string * string list * (string * typ) list
  | EnumDecl of string * string list
  | ExternDecl of func_def
  | GenericFuncDecl of string * string list * func_def
  | NamespaceDecl of string * decl list

let c_symbol_name name =
  let b = Buffer.create (String.length name) in
  let rec loop i =
    if i >= String.length name then ()
    else if i + 1 < String.length name && name.[i] = ':' && name.[i + 1] = ':' then
      (Buffer.add_string b "__"; loop (i + 2))
    else (Buffer.add_char b name.[i]; loop (i + 1))
  in
  loop 0;
  Buffer.contents b

let builtin_funcs = [
  ("alloc_ints", ([TInt], TPtr TInt));
  ("free_ints", ([TPtr TInt], TVoid));
  ("grow_ints", ([TPtr TInt; TInt; TInt], TPtr TInt));
  ("open_file", ([TString; TString], TPtr TVoid));
  ("read_char", ([TPtr TVoid], TInt));
  ("close_file", ([TPtr TVoid], TInt));
  ("write_char", ([TPtr TVoid; TInt], TInt));
  ("write_string", ([TPtr TVoid; TString], TInt));
  ("basalt_include_open_root", ([TString], TPtr TVoid));
  ("basalt_include_open_line", ([TPtr TInt; TInt; TInt], TPtr TVoid));
  ("basalt_include_last_status", ([], TInt));
  ("basalt_include_close", ([], TVoid));
  ("basalt_include_reset_session", ([], TVoid));
  ("basalt_include_line_mode", ([TPtr TInt; TInt], TInt));
  ("basalt_inc_realpath", ([TString], TString));
  ("basalt_inc_join", ([TString; TString], TString))
]

type program = {
  globals : global_def list;
  consts : global_def list;
  functions : func_def list;
  externs : func_def list;
  structs : (string * (string * typ) list) list;
  generic_structs : (string * string list * (string * typ) list) list;
  enums : (string * string list) list;
  generic_functions : (string * string list * func_def) list;
}


let rec genericize_type params = function
  | TNamed name when List.mem name params -> TParam name
  | TPtr t -> TPtr (genericize_type params t)
  | TArray (t, n) -> TArray (genericize_type params t, n)
  | TDynArray t -> TDynArray (genericize_type params t)
  | TGeneric (name, args) -> TGeneric (name, List.map (genericize_type params) args)
  | TFunPtr (args, ret) -> TFunPtr (List.map (genericize_type params) args, genericize_type params ret)
  | t -> t

let rec genericize_stmt params = function
  | Let (name, t, e) -> Let (name, genericize_type params t, e)
  | Const (name, t, e) -> Const (name, genericize_type params t, e)
  | Assign (l, r) -> Assign (l, r)
  | Print e -> Print e
  | IfStmt (c, t, e) -> IfStmt (c, genericize_stmt params t, genericize_stmt params e)
  | While (c, body) -> While (c, genericize_stmt params body)
  | For (init, cond, step, body) ->
      For (Option.map (genericize_stmt params) init, cond, Option.map (genericize_stmt params) step, genericize_stmt params body)
  | Block ss -> Block (List.map (genericize_stmt params) ss)
  | Return e -> Return e
  | ExprStmt e -> ExprStmt e
  | Break -> Break
  | Continue -> Continue

let genericize_func params f =
  { f with
    params = List.map (fun (name, t) -> (name, genericize_type params t)) f.params;
    return_type = genericize_type params f.return_type;
    body = genericize_stmt params f.body }

let qualify_name ns name = if ns = "" then name else ns ^ "::" ^ name

let rec qualify_type ns local_types = function
  | TPtr t -> TPtr (qualify_type ns local_types t)
  | TArray (t, n) -> TArray (qualify_type ns local_types t, n)
  | TDynArray t -> TDynArray (qualify_type ns local_types t)
  | TFunPtr (args, ret) -> TFunPtr (List.map (qualify_type ns local_types) args, qualify_type ns local_types ret)
  | TNamed n when List.mem n local_types -> TNamed (qualify_name ns n)
  | TGeneric (n, args) when List.mem n local_types -> TGeneric (qualify_name ns n, List.map (qualify_type ns local_types) args)
  | TGeneric (n, args) -> TGeneric (n, List.map (qualify_type ns local_types) args)
  | t -> t

let rec qualify_expr ns local_funcs = function
  | Binop (op, a, b) -> Binop (op, qualify_expr ns local_funcs a, qualify_expr ns local_funcs b)
  | Call (name, args) -> Call ((if List.mem name local_funcs then qualify_name ns name else name), List.map (qualify_expr ns local_funcs) args)
  | IndirectCall (f, args) -> IndirectCall (qualify_expr ns local_funcs f, List.map (qualify_expr ns local_funcs) args)
  | Deref e -> Deref (qualify_expr ns local_funcs e)
  | Index (e, i) -> Index (qualify_expr ns local_funcs e, qualify_expr ns local_funcs i)
  | AddressOf e -> AddressOf (qualify_expr ns local_funcs e)
  | Field (e, f) -> Field (qualify_expr ns local_funcs e, f)
  | e -> e

let rec qualify_stmt ns local_types local_funcs = function
  | Let (name, t, e) -> Let (name, qualify_type ns local_types t, qualify_expr ns local_funcs e)
  | Const (name, t, e) -> Const (name, qualify_type ns local_types t, qualify_expr ns local_funcs e)
  | Assign (l, r) -> Assign (qualify_expr ns local_funcs l, qualify_expr ns local_funcs r)
  | Print e -> Print (qualify_expr ns local_funcs e)
  | IfStmt (c, t, e) -> IfStmt (qualify_expr ns local_funcs c, qualify_stmt ns local_types local_funcs t, qualify_stmt ns local_types local_funcs e)
  | While (c, b) -> While (qualify_expr ns local_funcs c, qualify_stmt ns local_types local_funcs b)
  | For (init, c, step, body) -> For (Option.map (qualify_stmt ns local_types local_funcs) init, qualify_expr ns local_funcs c, Option.map (qualify_stmt ns local_types local_funcs) step, qualify_stmt ns local_types local_funcs body)
  | Block ss -> Block (List.map (qualify_stmt ns local_types local_funcs) ss)
  | Return e -> Return (Option.map (qualify_expr ns local_funcs) e)
  | ExprStmt e -> ExprStmt (qualify_expr ns local_funcs e)
  | Break -> Break
  | Continue -> Continue

let qualify_func ns local_types local_funcs f =
  { name = qualify_name ns f.name;
    params = List.map (fun (name, t) -> (name, qualify_type ns local_types t)) f.params;
    return_type = qualify_type ns local_types f.return_type;
    body = qualify_stmt ns local_types local_funcs f.body }

let rec qualify_namespace ns ds =
  let local_types = List.filter_map (function
    | StructDecl (n, _) | EnumDecl (n, _) -> Some n
    | GenericStructDecl (n, _, _) -> Some n
    | _ -> None) ds in
  let local_funcs = List.filter_map (function
    | FDecl f | ExternDecl f -> Some f.name
    | GenericFuncDecl (n, _, _) -> Some n
    | _ -> None) ds in
  List.concat_map (function
    | GDecl (name, t, e) -> [GDecl (qualify_name ns name, qualify_type ns local_types t, qualify_expr ns local_funcs e)]
    | GConst (name, t, e) -> [GConst (qualify_name ns name, qualify_type ns local_types t, qualify_expr ns local_funcs e)]
    | FDecl f -> [FDecl (qualify_func ns local_types local_funcs f)]
    | ExternDecl f -> [ExternDecl (qualify_func ns local_types local_funcs f)]
    | StructDecl (name, fields) -> [StructDecl (qualify_name ns name, List.map (fun (n, t) -> (n, qualify_type ns local_types t)) fields)]
    | GenericStructDecl (name, ps, fields) -> [GenericStructDecl (qualify_name ns name, ps, List.map (fun (n, t) -> (n, qualify_type ns local_types t)) fields)]
    | EnumDecl (name, values) -> [EnumDecl (qualify_name ns name, values)]
    | GenericFuncDecl (name, ps, f) -> [GenericFuncDecl (qualify_name ns name, ps, qualify_func ns local_types local_funcs f)]
    | NamespaceDecl (inner, inner_ds) -> qualify_namespace (qualify_name ns inner) inner_ds) ds

let flatten_decl = function
  | NamespaceDecl (ns, ds) -> qualify_namespace ns ds
  | d -> [d]
