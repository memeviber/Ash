{
open Parser
}

let white = [' ' '\t' '\n' '\r']+
let digit = ['0'-'9']
let real = ['0'-'9']+ '.' ['0'-'9']+
let id = ['a'-'z' 'A'-'Z' '_'] ['a'-'z' 'A'-'Z' '0'-'9' '_']*

rule read =
  parse
  | white    { read lexbuf }
  | "let"    { LET }
  | "const"  { CONST }
  | "null"   { NULL }
  | "func"   { FUNC }
  | "extern" { EXTERN }
  | "fn"     { FN }
  | "return" { RETURN }
  | "while"  { WHILE }
  | "for"    { FOR }
  | "break"  { BREAK }
  | "continue" { CONTINUE }
  | "if"     { IF }
  | "then"   { THEN }
  | "else"   { ELSE }
  | "print"  { PRINT }
  | "struct" { STRUCT }
  | "enum" { ENUM }
  | "namespace" { NAMESPACE }
  | "int"    { TINT }
  | "bool"   { TBOOL }
  | "float"  { TFLOAT }
  | "double" { TDOUBLE }
  | "string" { TSTRING }
  | "void"   { TVOID }
  | "char"   { TCHAR }
  | "array"  { ARRAY }
  | "true"   { TRUE }
  | "false"  { FALSE }
  | '"'      { read_string "" lexbuf }
  | '\''    { read_char lexbuf }
  | id       { ID (Lexing.lexeme lexbuf) }
  | real     { FLOAT (float_of_string (Lexing.lexeme lexbuf)) }
  | digit+   { INT (int_of_string (Lexing.lexeme lexbuf)) }
  | "=="     { EQ_EQ }
  | "!="     { NEQ }
  | "&&"     { AND_AND }
  | "||"     { OR_OR }
  | "<<"     { SHL }
  | ">>"     { SHR }
  | "^"      { BIT_XOR }
  | "|"      { BIT_OR }
  | "++"     { CONCAT }
  | "+"      { PLUS }
  | "-"      { MINUS }
  | "*"      { STAR }
  | "//" [^ '\n']* { read lexbuf }
  | "/"      { DIVIDE }
  | "%"      { MOD }
  | "<"      { LT }
  | ">"      { GT }
  | "="      { EQUAL }
  | "::"     { COLONCOLON }
  | ":"      { COLON }
  | "("      { LPAREN }
  | ")"      { RPAREN }
  | "{"      { LBRACE }
  | "}"      { RBRACE }
  | "["      { LBRACK }
  | "]"      { RBRACK }
  | "&"      { AMP }
  | "~"      { BIT_NOT }
  | "."      { DOT }
  | ","      { COMMA }
  | ";"      { SEMI }
  | eof      { EOF }
  | _        { failwith ("Unexpected character: " ^ Lexing.lexeme lexbuf) }

and read_char =
  parse
  | '\\' 'n' '\'' { CHAR '\n' }
  | '\\' 't' '\'' { CHAR '\t' }
  | '\\' 'r' '\'' { CHAR '\r' }
  | '\\' 'b' '\'' { CHAR '\b' }
  | '\\' 'f' '\'' { CHAR '\012' }
  | '\\' 'v' '\'' { CHAR '\011' }
  | '\\' '0' '\'' { CHAR '\000' }
  | '\\' '\\' '\'' { CHAR '\\' }
  | '\\' '\'' '\'' { CHAR '\'' }
  | '\\' '"' '\'' { CHAR '"' }
  | [^ '\'' '\\'] '\'' { CHAR (String.get (Lexing.lexeme lexbuf) 0) }
  | _ { failwith "Invalid character literal" }
and read_string buf =
  parse
  | '"'      { STRING buf }
  | '\\' 'n' { read_string (buf ^ "\n") lexbuf }
  | '\\' 't' { read_string (buf ^ "\t") lexbuf }
  | '\\' 'r' { read_string (buf ^ "\r") lexbuf }
  | '\\' 'b' { read_string (buf ^ "\b") lexbuf }
  | '\\' 'f' { read_string (buf ^ "\012") lexbuf }
  | '\\' 'v' { read_string (buf ^ "\011") lexbuf }
  | '\\' '0' { read_string (buf ^ "\000") lexbuf }
  | '\\' '"' { read_string (buf ^ "\\\"") lexbuf }
  | '\\' '\\' { read_string (buf ^ "\\\\") lexbuf }
  | [^ '"' '\\']+ { read_string (buf ^ Lexing.lexeme lexbuf) lexbuf }
  | _ { failwith "Unterminated string" }
