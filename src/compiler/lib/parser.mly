%{
open Ast
%}

%token <int> INT
%token <float> FLOAT
%token <char> CHAR
%token <string> ID STRING
%token TRUE FALSE
%token LET CONST NULL FUNC FN EXTERN RETURN WHILE FOR BREAK CONTINUE IF THEN ELSE PRINT STRUCT ENUM NAMESPACE
%token TINT TBOOL TCHAR TSTRING TFLOAT TDOUBLE TVOID ARRAY
%token PLUS MINUS STAR DIVIDE MOD CONCAT AND_AND OR_OR BIT_OR BIT_XOR BIT_NOT SHL SHR EQUAL EQ_EQ NEQ LT GT COLON COLONCOLON
%token LPAREN RPAREN LBRACE RBRACE SEMI COMMA AMP LBRACK RBRACK DOT
%token EOF

%nonassoc ELSE
%left OR_OR
%left AND_AND
%left BIT_OR
%left BIT_XOR
%left EQ_EQ NEQ LT GT
%left SHL SHR
%left CONCAT
%left PLUS MINUS
%left STAR DIVIDE MOD
%nonassoc BIT_NOT AMP
%right UMINUS

%start <Ast.program> prog

%%

prog:
  | raw_ds = list(decl); EOF {
      let ds = List.concat_map flatten_decl raw_ds in
      let gs = List.filter_map (function GDecl g -> Some g | _ -> None) ds in
      let cs = List.filter_map (function GConst g -> Some g | _ -> None) ds in
      let fs = List.filter_map (function FDecl f -> Some f | _ -> None) ds in
      let xs = List.filter_map (function ExternDecl f -> Some f | _ -> None) ds in
      let ss = List.filter_map (function StructDecl (n, fs) -> Some (n, fs) | _ -> None) ds in
      let gss = List.filter_map (function GenericStructDecl (n, ps, fs) -> Some (n, ps, fs) | _ -> None) ds in
      let es = List.filter_map (function EnumDecl (n, vs) -> Some (n, vs) | _ -> None) ds in
      let gfs = List.filter_map (function GenericFuncDecl (n, ps, f) -> Some (n, ps, f) | _ -> None) ds in
      { globals = gs; consts = cs; functions = fs; externs = xs; structs = ss; generic_structs = gss; enums = es; generic_functions = gfs }
    }

decl:
  | NAMESPACE; name = name_atom; LBRACE; ds = list(decl); RBRACE { NamespaceDecl (name, ds) }
  | STRUCT; name = ID; LT; ps = separated_nonempty_list(COMMA, ID); GT; LBRACE; fs = list(struct_field); RBRACE {
      GenericStructDecl (name, ps, List.map (fun (f, t) -> (f, genericize_type ps t)) fs)
    }
  | STRUCT; name = ID; LBRACE; fs = list(struct_field); RBRACE { StructDecl (name, fs) }
  | ENUM; name = ID; LBRACE; vs = separated_list(COMMA, ID); RBRACE { EnumDecl (name, vs) }
  | FUNC; name = ID; LT; ps = separated_nonempty_list(COMMA, ID); GT; LPAREN; p = separated_list(COMMA, param); RPAREN; COLON; rt = typ; LBRACE; body = list(stmt); RBRACE {
      let f = { name; params = List.map (fun (x, t) -> (x, genericize_type ps t)) p; return_type = genericize_type ps rt; body = Block (List.map (genericize_stmt ps) body) } in
      GenericFuncDecl (name, ps, f)
    }
  | LET; x = ID; COLON; t = typ; EQUAL; e = expr; SEMI { GDecl (x, t, e) }
  | CONST; x = ID; COLON; t = typ; EQUAL; e = expr; SEMI { GConst (x, t, e) }
  | FUNC; name = ID; LPAREN; p = separated_list(COMMA, param); RPAREN; COLON; rt = typ; LBRACE; body = list(stmt); RBRACE { 
      FDecl { name; params = p; return_type = rt; body = Block body } 
    }
  | EXTERN; FUNC; name = ID; LPAREN; p = separated_list(COMMA, param); RPAREN; COLON; rt = typ; SEMI {
      ExternDecl { name; params = p; return_type = rt; body = Block [] }
    }

param:
  | x = ID; COLON; t = typ { (x, t) }

struct_field:
  | x = ID; COLON; t = typ; SEMI { (x, t) }

typ:
  | x = qualified_name; LT; ts = separated_nonempty_list(COMMA, typ); GT { TGeneric (x, ts) }
  | TINT { TInt }
  | TBOOL { TBool }
  | TCHAR { TChar }
  | TSTRING { TString }
  | TFLOAT { TFloat }
  | TDOUBLE { TDouble }
  | TVOID { TVoid }
  | x = qualified_name { TNamed x }
  | FN; LPAREN; ps = separated_list(COMMA, typ); RPAREN; COLON; r = typ { TFunPtr (ps, r) }
  | t = typ; STAR { TPtr t }
  | t = typ; LBRACK; n = INT; RBRACK { TArray(t, n) }

stmt:
  | LET; x = ID; COLON; t = typ; EQUAL; e = expr; SEMI { Let(x, t, e) }
  | CONST; x = ID; COLON; t = typ; EQUAL; e = expr; SEMI { Const(x, t, e) }
  | lv = expr; EQUAL; e = expr; SEMI { Assign(lv, e) }
  | PRINT; e = expr; SEMI { Print(e) }
  | BREAK; SEMI { Break }
  | CONTINUE; SEMI { Continue }
  | IF; c = expr; THEN; t = stmt; ELSE; e = stmt { IfStmt(c, t, e) }
  | IF; c = expr; THEN; t = stmt { IfStmt(c, t, Block []) }
  | WHILE; c = expr; LBRACE; ss = list(stmt); RBRACE { While(c, Block ss) }
  | FOR; LPAREN; init = option(for_init); SEMI; c = expr; SEMI; step = option(for_step); RPAREN; LBRACE; ss = list(stmt); RBRACE {
      For(init, c, step, Block ss)
    }
  | LBRACE; ss = list(stmt); RBRACE { Block(ss) }
  | RETURN; e = option(expr); SEMI { Return e }
  | e = expr; SEMI { ExprStmt e }

for_init:
  | LET; x = ID; COLON; t = typ; EQUAL; e = expr { Let(x, t, e) }
  | lv = expr; EQUAL; e = expr { Assign(lv, e) }

for_step:
  | lv = expr; EQUAL; e = expr { Assign(lv, e) }
  | e = expr { ExprStmt e }

name_atom:
  | x = ID { x }
  | ARRAY { "array" }

qualified_name:
  | x = name_atom { x }
  | x = qualified_name; COLONCOLON; y = name_atom { x ^ "::" ^ y }

expr:
  | MINUS; e = expr %prec UMINUS { Binop (Sub, Int 0, e) }
  | i = INT { Int i }
  | f = FLOAT { Float f }
  | c = CHAR { Char c }
  | NULL { Null }
  | s = STRING { String s }
  | TRUE { Bool true }
  | FALSE { Bool false }
  | x = qualified_name { Var x }
  | f = qualified_name; LPAREN; args = separated_list(COMMA, expr); RPAREN { Call(f, args) }
  | LPAREN; f = expr; RPAREN; LPAREN; args = separated_list(COMMA, expr); RPAREN { IndirectCall(f, args) }
  | STAR; e = expr { Deref e }
  | AMP; e = expr { AddressOf e }
  | BIT_NOT; e = expr { Binop(BitXor, e, Int (-1)) }
  | e = expr; LBRACK; idx = expr; RBRACK { Index(e, idx) }
  | e = expr; DOT; f = ID { Field(e, f) }
  | e1 = expr; OR_OR; e2 = expr { Binop(Or, e1, e2) }
  | e1 = expr; BIT_OR; e2 = expr { Binop(BitOr, e1, e2) }
  | e1 = expr; BIT_XOR; e2 = expr { Binop(BitXor, e1, e2) }
  | e1 = expr; SHL; e2 = expr { Binop(Shl, e1, e2) }
  | e1 = expr; SHR; e2 = expr { Binop(Shr, e1, e2) }
  | e1 = expr; AND_AND; e2 = expr { Binop(And, e1, e2) }
  | e1 = expr; AMP; e2 = expr { Binop(BitAnd, e1, e2) }
  | e1 = expr; CONCAT; e2 = expr { Binop(Concat, e1, e2) }
  | e1 = expr; PLUS; e2 = expr { Binop(Add, e1, e2) }
  | e1 = expr; MINUS; e2 = expr { Binop(Sub, e1, e2) }
  | e1 = expr; STAR; e2 = expr { Binop(Mul, e1, e2) }
  | e1 = expr; DIVIDE; e2 = expr { Binop(Div, e1, e2) }
  | e1 = expr; MOD; e2 = expr { Binop(Mod, e1, e2) }
  | e1 = expr; EQ_EQ; e2 = expr { Binop(Eq, e1, e2) }
  | e1 = expr; NEQ; e2 = expr { Binop(Neq, e1, e2) }
  | e1 = expr; LT; e2 = expr { Binop(Lt, e1, e2) }
  | e1 = expr; GT; e2 = expr { Binop(Gt, e1, e2) }
  | LPAREN; e = expr; RPAREN { e }

%%
