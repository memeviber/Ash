#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#endif

#if defined(_WIN32)
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
#define ASH_UNUSED __attribute__((unused))
#else
#define ASH_UNUSED
#endif
static void* ash_track(void*);
static char** ash_inc_active=NULL;static size_t ash_inc_active_n=0,ash_inc_active_cap=0;static char** ash_inc_loaded=NULL;static size_t ash_inc_loaded_n=0,ash_inc_loaded_cap=0;static int ash_inc_status=0;
static ASH_UNUSED size_t ash_inc_find(char**v,size_t n,const char*p){size_t i;for(i=0;i<n;i++)if(strcmp(v[i],p)==0)return i;return (size_t)-1;}
static ASH_UNUSED void ash_inc_add(char***vp,size_t*np,size_t*cp,char*p){size_t c;char**q;if(*np==*cp){c=*cp?*cp*2:16;q=(char**)realloc(*vp,c*sizeof(char*));if(!q)exit(2);*vp=q;*cp=c;}(*vp)[(*np)++]=p;}
static ASH_UNUSED char* ash_inc_strdup(const char*p){size_t n=strlen(p);char*q=(char*)malloc(n+1);if(!q)exit(2);memcpy(q,p,n+1);return(char*)ash_track(q);}
static ASH_UNUSED char* ash_inc_realpath(const char*p){
#if defined(_WIN32)
 char*q=_fullpath(NULL,p,0);if(q)return(char*)ash_track(q);
#else
 char*q=realpath(p,NULL);if(q)return(char*)ash_track(q);
#endif
 return ash_inc_strdup(p);
}
static ASH_UNUSED int ash_inc_begin(char*p){if(ash_inc_find(ash_inc_active,ash_inc_active_n,p)!=(size_t)-1){ash_inc_status=1;return 0;}if(ash_inc_find(ash_inc_loaded,ash_inc_loaded_n,p)!=(size_t)-1){ash_inc_status=2;return 0;}ash_inc_add(&ash_inc_active,&ash_inc_active_n,&ash_inc_active_cap,p);ash_inc_status=0;return 1;}
static ASH_UNUSED void ash_include_close(void){if(ash_inc_active_n){char*p=ash_inc_active[--ash_inc_active_n];if(ash_inc_find(ash_inc_loaded,ash_inc_loaded_n,p)==(size_t)-1)ash_inc_add(&ash_inc_loaded,&ash_inc_loaded_n,&ash_inc_loaded_cap,p);}}
static ASH_UNUSED char* ash_inc_join(const char*base,const char*raw){const char*s=strrchr(base,'/');size_t n=s?(size_t)(s-base+1):0;size_t m=strlen(raw);char*q=(char*)malloc(n+m+1);if(!q)exit(2);if(n)memcpy(q,base,n);memcpy(q+n,raw,m+1);return(char*)ash_track(q);}
static ASH_UNUSED int ash_include_line_mode(int*line,int n){int i=0,j;while(i<n&&(line[i]==' '||line[i]=='\t'))i++;if(i+7<=n&&!memcmp(line+i,"include",7)){j=i+7;while(j<n&&(line[j]==' '||line[j]=='\t'))j++;if(j<n&&line[j]=='\"')return 1;}if(i+8<=n&&!memcmp(line+i,"includec",8)){j=i+8;while(j<n&&(line[j]==' '||line[j]=='\t'))j++;if(j<n&&line[j]=='\"')return 2;}return 0;}
static ASH_UNUSED void* ash_include_open_root(const char*path){char*p=ash_inc_realpath(path);FILE*f;if(!ash_inc_begin(p))return NULL;f=fopen(p,"r");if(!f){ash_inc_status=3;ash_include_close();return NULL;}return(void*)f;}
static ASH_UNUSED void* ash_include_open_line(int*line,int n,int mode){int i=0,a,b,j;char*raw,*joined,*canon;FILE*f;(void)mode;while(i<n&&line[i]!='"')i++;if(i>=n)return NULL;a=++i;while(i<n&&line[i]!='"')i++;if(i>=n)return NULL;b=i;raw=(char*)malloc((size_t)(b-a)+1);if(!raw)exit(2);{int k;for(k=0;k<b-a;k++)raw[k]=(char)line[a+k];}raw[b-a]=0;raw=(char*)ash_track(raw);j=i+1;while(j<n&&(line[j]==' '||line[j]=='\t'))j++;if(j<n&&line[j]==';')j++;while(j<n&&(line[j]==' '||line[j]=='\t'))j++;if(j!=n)return NULL;joined=ash_inc_join(ash_inc_active[ash_inc_active_n-1],raw);canon=ash_inc_realpath(joined);if(!ash_inc_begin(canon))return NULL;f=fopen(canon,"r");if(!f){ash_inc_status=3;ash_include_close();return NULL;}return(void*)f;}
static ASH_UNUSED int ash_include_last_status(void){return ash_inc_status;}
static ASH_UNUSED void ash_include_reset_session(void){ash_inc_active_n=0;ash_inc_loaded_n=0;ash_inc_status=0;}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static ASH_UNUSED void* open_file(const char* path, const char* mode) {
  return (void*)fopen(path, mode);
}
static ASH_UNUSED int read_char(void* handle) {
  int c = fgetc((FILE*)handle);
  return c == EOF ? -1 : c;
}
static ASH_UNUSED int close_file(void* handle) {
  return fclose((FILE*)handle);
}
static ASH_UNUSED int write_char(void* handle, int c) {
  return fputc(c, (FILE*)handle);
}
static ASH_UNUSED int write_string(void* handle, const char* s) {
  return fputs(s, (FILE*)handle);
}
static void** ash_live = NULL; static size_t ash_live_n = 0, ash_live_cap = 0;
static ASH_UNUSED void ash_panic(int code) { fprintf(stderr, "Ash memory error %d\n", code); exit(2); }
static ASH_UNUSED size_t ash_checked_bytes(int count, size_t elem_size) { if (count < 0) ash_panic(1); if (elem_size != 0 && (size_t)count > (size_t)-1 / elem_size) ash_panic(1); return (size_t)count * elem_size; }
static ASH_UNUSED size_t ash_find(void* p) { size_t i; for (i = 0; i < ash_live_n; ++i) if (ash_live[i] == p) return i; return (size_t)-1; }
static ASH_UNUSED void ash_validate(void) { size_t i, j; for (i = 0; i < ash_live_n; ++i) { if (!ash_live[i]) ash_panic(2); for (j = i + 1; j < ash_live_n; ++j) if (ash_live[i] == ash_live[j]) ash_panic(2); } }
static ASH_UNUSED void ash_cleanup(void) { size_t i; ash_validate(); for (i = 0; i < ash_live_n; ++i) free(ash_live[i]); free(ash_live); ash_live = NULL; ash_live_n = ash_live_cap = 0; }
static ASH_UNUSED void* ash_track(void* p) { size_t c; void** q; if (!p) return NULL; if (ash_find(p) != (size_t)-1) ash_panic(2); if (ash_live_n == ash_live_cap) { if (ash_live_cap > (size_t)-1 / 2) ash_panic(2); c = ash_live_cap ? ash_live_cap * 2 : 32; if (c > (size_t)-1 / sizeof(void*)) ash_panic(2); q = (void**)realloc(ash_live, c * sizeof(void*)); if (!q) ash_panic(2); ash_live = q; ash_live_cap = c; } ash_live[ash_live_n++] = p; atexit(ash_cleanup); return p; }
static ASH_UNUSED void ash_release(void* p) { size_t i; if (!p) return; i = ash_find(p); if (i == (size_t)-1) ash_panic(2); free(p); ash_live[i] = ash_live[--ash_live_n]; }
typedef struct AshArray { void* data; int len; int cap; } AshArray;
static ASH_UNUSED void ash_array_check(const AshArray* a) { if (!a) ash_panic(4); if (a->len < 0 || a->cap < 0 || a->len > a->cap) ash_panic(3); if (a->cap > 0 && (!a->data || ash_find(a->data) == (size_t)-1)) ash_panic(2); }
static ASH_UNUSED AshArray runtime_array_make(int capacity, size_t elem_size) { AshArray a; if (capacity < 0) ash_panic(1); if (capacity < 1) capacity = 4; ash_checked_bytes(capacity, elem_size); a.data = calloc((size_t)capacity, elem_size); if (!a.data) ash_panic(5); a.len = 0; a.cap = capacity; a.data = ash_track(a.data); return a; }
static ASH_UNUSED AshArray runtime_array_reserve(AshArray* a, int minimum, size_t elem_size) { void* p; ash_array_check(a); if (minimum < 0) ash_panic(1); if (minimum <= a->cap) return *a; ash_checked_bytes(minimum, elem_size); p = calloc((size_t)minimum, elem_size); if (!p) ash_panic(5); if (a->data) { memcpy(p, a->data, ash_checked_bytes(a->len, elem_size)); ash_release(a->data); } a->data = ash_track(p); a->cap = minimum; return *a; }
static ASH_UNUSED AshArray runtime_array_push(AshArray* a, const void* value, size_t elem_size) { int next; ash_array_check(a); if (!value) ash_panic(4); if (a->len >= a->cap) { if (a->cap > 2147483647 / 2) ash_panic(1); next = a->cap > 0 ? a->cap * 2 : 4; runtime_array_reserve(a, next, elem_size); } memcpy((char*)a->data + ash_checked_bytes(a->len, elem_size), value, elem_size); a->len += 1; return *a; }
static ASH_UNUSED void runtime_array_set(AshArray* a, int index, const void* value, size_t elem_size) { ash_array_check(a); if (index < 0 || index >= a->len) ash_panic(3); if (!value) ash_panic(4); memcpy((char*)a->data + ash_checked_bytes(index, elem_size), value, elem_size); }
static ASH_UNUSED AshArray runtime_array_clear(AshArray* a) { ash_array_check(a); a->len = 0; return *a; }
static ASH_UNUSED void runtime_array_free(AshArray* a) { if (!a) ash_panic(4); if (a->data) { if (ash_find(a->data) == (size_t)-1) ash_panic(2); ash_release(a->data); } a->data = NULL; a->len = 0; a->cap = 0; }
static ASH_UNUSED void* runtime_array_get_raw(AshArray* a, int index, size_t elem_size) { ash_array_check(a); if (index < 0 || index >= a->len) ash_panic(3); return (char*)a->data + ash_checked_bytes(index, elem_size); }
static ASH_UNUSED char* runtime_string_concat(const char* a, const char* b) {
  size_t na, nb, total; char* p; if (!a || !b) ash_panic(4); na = strlen(a); nb = strlen(b); if (na > (size_t)-1 - nb - 1) ash_panic(1); total = na + nb + 1; p = (char*)malloc(total); if (!p) ash_panic(5); memcpy(p, a, na); memcpy(p + na, b, nb); p[na + nb] = 0; return (char*)ash_track(p);
}
static ASH_UNUSED int write_int(int* handle, int value) {
  return fprintf((FILE*)handle, "%d", value);
}
static ASH_UNUSED int* alloc_ints(int count) { int* p; if (count < 0) ash_panic(1); if (count < 1) count = 1; ash_checked_bytes(count, sizeof(int)); p = (int*)calloc((size_t)count, sizeof(int)); if (!p) ash_panic(5); return (int*)ash_track(p); }
static ASH_UNUSED void free_ints(int* p) { ash_release(p); }
static ASH_UNUSED int* grow_ints(int* old, int old_count, int new_count) { size_t slot = (size_t)-1; int* p; if (old_count < 0 || new_count < 0) ash_panic(1); if (new_count <= old_count) return old; if (old) { slot = ash_find(old); if (slot == (size_t)-1) ash_panic(2); } ash_checked_bytes(new_count, sizeof(int)); p = (int*)realloc(old, (size_t)new_count * sizeof(int)); if (!p) ash_panic(6); if (old) ash_live[slot] = p; else ash_track(p); memset(p + old_count, 0, (size_t)(new_count - old_count) * sizeof(int)); return p; }

int N_NONE = 0;
int N_INT = 1;
int N_BOOL = 2;
int N_STRING = 3;
int N_VAR = 4;
int N_BINOP = 5;
int N_CALL = 6;
int N_DEREF = 7;
int N_INDEX = 8;
int N_ADDRESS = 9;
int N_LET = 10;
int N_ASSIGN = 11;
int N_PRINT = 12;
int N_IF = 13;
int N_WHILE = 14;
int N_BLOCK = 15;
int N_RETURN = 16;
int N_GLOBAL = 17;
int N_PARAM = 18;
int N_FUNC = 19;
int N_PROGRAM = 20;
int N_LIST = 21;
int N_EXPR = 22;
int N_BREAK = 23;
int N_CONTINUE = 24;
int N_FOR = 25;
int N_STRUCT = 26;
int N_ENUM = 27;
int N_FIELD = 28;
int N_FIELD_ACCESS = 29;
int N_CHAR = 30;
int N_NULL = 31;
int N_CONST = 32;
int N_FLOAT = 33;
int N_EXTERN = 34;
int N_GENERIC_STRUCT = 35;
int N_GENERIC_FUNC = 36;
int OP_ADD = 1;
int OP_SUB = 2;
int OP_MUL = 3;
int OP_DIV = 4;
int OP_EQ = 5;
int OP_NEQ = 6;
int OP_LT = 7;
int OP_GT = 8;
int OP_AND = 9;
int OP_OR = 10;
int OP_CONCAT = 11;
int OP_BITAND = 12;
int OP_BITOR = 13;
int OP_BITXOR = 14;
int OP_SHL = 15;
int OP_SHR = 16;
int OP_MOD = 17;
int TY_INT = 1;
int TY_BOOL = 2;
int TY_STRING = 3;
int TY_VOID = 4;
int TY_PTR = 5;
int TY_ARRAY = 6;
int TY_NAMED = 7;
int TY_CHAR = 8;
int TY_FLOAT = 9;
int TY_DOUBLE = 10;
int TY_FUN = 11;
int TY_PTRDIFF = 12;
int TY_DYN_ARRAY = 13;
int TY_PARAM = 14;
int TY_GENERIC = 15;
int* node_kind = 0;
int* node_a = 0;
int* node_b = 0;
int* node_c = 0;
int* node_next = 0;
int* node_value = 0;
int* node_aux = 0;
int* node_pos = 0;
int* node_scope = 0;
int node_count = 1;
int ast_namespace_scope = 0;
int node_cap = 0;
int payload_cap = 0;
int current_source_pos = 0;
int code_cap = 0;
int pipeline_root = 0;
int input_cap = 0;
int source_cap = 0;
int c_source_cap = 0;
int sym_cap = 0;
int* payload_int = 0;
int* payload_name = 0;
int* payload_string = 0;
int payload_count = 1;
int C_KW = 1;
int C_IDENT = 2;
int C_INT = 3;
int C_STRING = 4;
int C_OP = 5;
int C_PUNCT = 6;
int C_NEWLINE = 7;
int* code_kind = 0;
int* code_value = 0;
int code_count = 0;
int emit_for_step = 0;
int* gen_bind_name = 0;
int* gen_bind_type = 0;
int gen_bind_count = 0;
int gen_bind_cap = 0;
int gen_mangle_start = 0;
int gen_mangle_len = 0;
int* gen_spec_kind = 0;
int* gen_spec_decl = 0;
int* gen_spec_type = 0;
int* gen_spec_name = 0;
int gen_spec_count = 0;
int gen_spec_cap = 0;
int gen_name_override = 0;
int gen_debug = 0;
int T_EOF = 0;
int T_LET = 1;
int T_FUNC = 2;
int T_ID = 3;
int T_INT = 4;
int T_STRING = 5;
int T_TRUE = 6;
int T_FALSE = 7;
int T_RETURN = 8;
int T_WHILE = 9;
int T_FOR = 10;
int T_BREAK = 11;
int T_CONTINUE = 12;
int T_IF = 13;
int T_THEN = 14;
int T_ELSE = 15;
int T_PRINT = 16;
int T_TINT = 17;
int T_TBOOL = 18;
int T_TSTRING = 19;
int T_TVOID = 20;
int T_PLUS = 21;
int T_MINUS = 22;
int T_STAR = 23;
int T_DIVIDE = 24;
int T_CONCAT = 25;
int T_AND_AND = 26;
int T_OR_OR = 27;
int T_EQUAL = 28;
int T_EQEQ = 29;
int T_NEQ = 30;
int T_LT = 31;
int T_GT = 32;
int T_COLON = 33;
int T_LPAREN = 34;
int T_RPAREN = 35;
int T_LBRACE = 36;
int T_RBRACE = 37;
int T_SEMI = 38;
int T_COMMA = 39;
int T_AMP = 40;
int T_LBRACK = 41;
int T_RBRACK = 42;
int T_STRUCT = 43;
int T_ENUM = 44;
int T_DOT = 45;
int T_CHAR = 46;
int T_NULL = 47;
int T_CONST = 48;
int T_TCHAR = 49;
int T_FLOAT = 50;
int T_TDOUBLE = 51;
int T_BITOR = 52;
int T_BITXOR = 53;
int T_BITNOT = 54;
int T_SHL = 55;
int T_SHR = 56;
int T_FN = 57;
int T_EXTERN = 58;
int T_ARRAY = 59;
int T_NAMESPACE = 60;
int T_SCOPE = 61;
int T_MOD = 62;
int* input_kind = 0;
int* input_value = 0;
int input_count = 0;
int input_pos = 0;
int debug_tokens = 0;
int for_step_context = 0;
int ast_generic_scope = 0;
int L_EOF = 0;
int L_ID = 1;
int L_INT = 2;
int L_FUNC = 3;
int L_LET = 4;
int L_PRINT = 5;
int L_RETURN = 6;
int L_IF = 7;
int L_ELSE = 8;
int L_WHILE = 9;
int L_TRUE = 10;
int L_FALSE = 11;
int L_TINT = 12;
int L_TBOOL = 13;
int L_TSTRING = 14;
int L_STRING = 35;
int L_TVOID = 15;
int L_THEN = 37;
int L_AMP = 34;
int L_PLUS = 16;
int L_MINUS = 17;
int L_STAR = 18;
int L_DIV = 19;
int L_EQ = 20;
int L_EQEQ = 21;
int L_NEQ = 22;
int L_LT = 23;
int L_GT = 24;
int L_LPAREN = 25;
int L_RPAREN = 26;
int L_LBRACE = 27;
int L_RBRACE = 28;
int L_COLON = 29;
int L_SEMI = 30;
int L_COMMA = 31;
int L_LBRACK = 32;
int L_RBRACK = 33;
int L_FOR = 38;
int L_BREAK = 39;
int L_CONTINUE = 40;
int L_CONCAT = 41;
int L_AND = 42;
int L_OR = 43;
int L_STRUCT = 44;
int L_ENUM = 45;
int L_DOT = 46;
int L_CHAR = 47;
int L_NULL = 48;
int L_CONST = 49;
int L_TCHAR = 50;
int L_FLOAT = 51;
int L_TFLOAT = 52;
int L_TDOUBLE = 53;
int L_BITOR = 54;
int L_BITXOR = 55;
int L_BITNOT = 56;
int L_SHL = 57;
int L_SHR = 58;
int L_FN = 59;
int L_EXTERN = 60;
int L_ARRAY = 61;
int L_NAMESPACE = 62;
int L_SCOPE = 63;
int L_MOD = 64;
int* source = 0;
int source_len = 0;
int source_pos = 0;
int* c_source = 0;
int c_source_len = 0;
int include_ok = 1;
int include_line_cap = 4096;
int* sym_start = 0;
int* sym_len = 0;
int* sym_hash = 0;
int* sym_kind = 0;
int* sym_type = 0;
int* sym_elem_kind = 0;
int* sym_elem_name = 0;
int* sym_scope = 0;
int sym_count = 1;
int sym_text_len = 0;
int tok_kind = 0;
int tok_value = 0;
int tok_start = 0;
int tok_length = 0;
int tc_root = 0;
int tc_ok = 1;
int tc_error_code = 0;
int tc_error_symbol = 0;
int tc_error_pos = 0;
int tc_name = 0;
int tc_kind = 0;
int tc_elem_kind = 0;
int tc_elem_name = 0;
int tc_expected_elem_kind = 0;
int tc_expected_elem_name = 0;
int* tc_var_name = 0;
int* tc_var_kind = 0;
int* tc_var_named = 0;
int* tc_var_elem_kind = 0;
int* tc_var_elem_name = 0;
int* tc_var_type = 0;
int* tc_var_owned = 0;
int* tc_var_moved = 0;
int* tc_var_borrow_count = 0;
int* tc_var_borrow_source = 0;
int tc_last_var_type = 0;
int tc_last_var_owned = 0;
int tc_last_var_moved = 0;
int tc_last_var_index = 0;
int tc_expr_borrow_source = (0 - 1);
int tc_var_count = 0;
int tc_global_count = 0;
int tc_var_cap = 0;
int* tc_scope_start = 0;
int tc_scope_count = 0;
int tc_scope_cap = 0;
int* tc_path_name = 0;
int tc_path_count = 0;
int tc_path_cap = 0;
int tc_loop_depth = 0;
int tc_result_type = 0;
int* tc_bind_name = 0;
int* tc_bind_type = 0;
int tc_bind_count = 0;
int tc_bind_cap = 0;

int next_capacity(int old, int need);
void ensure_node(int need);
void ensure_payload(int need);
void ensure_code(int need);
void ensure_input(int need);
void ensure_source(int need);
void ensure_sym(int need);
int ast_node(int kind, int a, int b, int c, int value, int aux);
int ast_link(int head, int item);
int payload_make_int(int value);
int payload_make_name(int name_id);
int payload_make_string(int string_id);
void gen_bind_clear(void);
void ensure_gen_bind(int need);
int gen_bind_find(int name);
void gen_bind_add(int name, int ty);
void gen_append_char(int c);
void gen_append_char_text(char c);
void gen_append_text(char* text);
void gen_append_symbol(int id);
void gen_append_c_symbol(int id);
int sym_c_symbol(int id);
void gen_mangle_type(int ty);
int gen_mangled_type_symbol(int ty);
int gen_mangled_function_symbol(int base, int args);
void code_emit(int kind, int value);
void code_reset(void);
void ensure_gen_specs(int need);
int gen_substitute_type(int ty);
int gen_spec_exists(int kind, int decl, int name);
void gen_add_struct_spec(int ty);
void gen_add_fun_spec(int decl, int args);
void gen_collect_type(int ty);
void gen_collect_expr(int id);
void gen_collect_stmt(int id);
void gen_type(int kind, int child, int size);
int gen_array_elem_kind(int arg);
int gen_array_elem_name(int arg);
void gen_array_elem_type(int kind, int name);
void gen_array_sizeof(int kind, int name);
void gen_array_value_ptr(int kind, int name, int value);
void gen_array_make_expr(int capacity, int kind, int name);
void gen_array_checked_get(int arr, int pos, int kind, int name);
void gen_array_builtin(int id);
int gen_call_name(int id);
void gen_expr(int id);
int gen_expr_kind(int id);
void gen_for_clause(int id);
void gen_initializer(int ty, int expr);
void gen_fun_decl(int ty, int name);
void gen_decl(int ty, int name);
void gen_stmt(int id);
void gen_unify_formal(int formal, int actual);
void gen_bind_decl(int decl, int inst);
void gen_struct_decl_specialized(int decl, int inst, int cname);
void gen_function_specialized(int decl, int inst, int cname);
void gen_struct_decl(int id);
void gen_enum_decl(int id);
void gen_extern_param(int ty, int name);
void gen_function_signature(int id);
void gen_prototype(int id);
void gen_function(int id);
void gen_program(int id);
int build_regression_ast(void);
void generator_regression_main(void);
void input_reset(void);
void input_put(int kind, int value);
int input_peek(void);
int input_payload(void);
int input_take(int kind);
int ast_generic_param(int name);
int ast_generic_params(void);
int ast_type(void);
int ast_primary(void);
int ast_unary(void);
int ast_precedence(int kind);
int ast_operator(int kind);
int ast_expr_prec(int min_prec);
int ast_expr(void);
int clone_for_step(int step);
int lower_for_stmt(int id, int step);
int ast_stmt(void);
int ast_params(void);
int ast_struct_decl(void);
int ast_enum_decl(void);
int ast_namespace_decl(void);
int ast_decl(void);
int ast_program(void);
void c_source_reset(void);
void ensure_c_source(int need);
void c_source_put(int c);
void source_reset(void);
void source_put(int c);
int is_space(int c);
int is_digit(int c);
int is_alpha(int c);
int is_alnum(int c);
int source_peek(void);
int source_take(void);
int span_hash(int start, int length);
int span_equal(int a, int b, int length);
int sym_lookup(int start, int length, int h);
int sym_qualified(int ns, int name);
int ast_decl_name(int name);
int ast_type_name(int name);
int sym_intern(int start, int length, int kind, int scope);
int word_code(int start, int length);
void lexer_skip(void);
int lexer_next(void);
void include_process_line(int* line, int length);
void include_expand_handle(int* handle);
void load_source_file(char* path);
int map_token(int k);
void load_tokens_from_file(char* path);
void ensure_tc_vars(int need);
void ensure_tc_scopes(int need);
void tc_enter_scope(void);
void tc_leave_scope(void);
void ensure_tc_path(int need);
void tc_fail(int code);
void ensure_tc_bindings(int need);
void tc_bind_clear(void);
int tc_bind_find(int name);
int tc_bind_add(int name, int ty);
int tc_type_equal(int a, int b);
int tc_type_node_from_summary(int kind, int name, int elem_kind, int elem_name);
void tc_match_generic(int formal, int actual);
int tc_substitute_type(int ty);
int tc_same(int a_kind, int a_name, int b_kind, int b_name);
int tc_array_elem_same(int a_kind, int a_name, int b_kind, int b_name);
int tc_same_full(int a_kind, int a_name, int a_elem_kind, int a_elem_name, int b_kind, int b_name, int b_elem_kind, int b_elem_name);
int tc_ptr_diff_ok(int a_kind, int a_elem_kind, int a_elem_name, int b_kind, int b_elem_kind, int b_elem_name);
int sym_suffix_equal(int full, int base);
int tc_find_struct(int name);
int tc_find_struct_ctx(int name, int ns);
int tc_find_enum(int name);
int tc_find_enum_ctx(int name, int ns);
int tc_generic_arity(int decl);
int tc_generic_arg_count(int ty);
int tc_named_exists_ctx(int name, int ns);
int tc_named_exists(int name);
void tc_check_type(int ty);
int tc_cycle_struct(int name);
int tc_cycle_type(int ty);
int tc_name_is_array_free(int name);
int tc_release_name(int name);
int tc_owned_initializer(int id);
int tc_is_owner_kind(int kind);
int tc_borrow_conflict(int index);
void tc_move_var(int index);
void tc_move_value(int id);
void tc_record_borrow(int destination, int source_index2);
void tc_require_mutable(int id);
void tc_consume_call(int id);
void tc_add_var(int name, int kind, int named, int elem_kind, int elem_name, int type_node);
int tc_lookup_var(int name);
void tc_type_node(int ty);
void tc_expr(int id);
int tc_find_function(int name);
int tc_find_function_ctx(int name, int ns);
int tc_find_enum_value(int name);
int tc_emit_arg_type(int id);
int tc_expr_kind_for_emit(int id);
void tc_stmt(int id, int expected_kind, int expected_name);
int tc_diag_line(int pos);
int tc_diag_col(int pos);
void tc_diag(void);
int tc_program(int root);
int pipeline_main(char* path);
void emit_symbol(int* out, int id);
void emit_string(int* out, int id);
void emit_print_prefix(int* out);
void emit_int_text(int* out, int value);
void emit_c_token(int* out, int kind, int value);
void emit_runtime(int* out);
void emit_c_file(char* path);
int main(int argc, char** argv);

int next_capacity(int old, int need) {
  int n = old;
  if ((n < 16)) {
    n = 16;
  } else {
    {
    }
  }
  while ((n < (need + 1))) {
    {
      n = (n * 2);
    }
  }
  return n;
}

void ensure_node(int need) {
  if ((need < node_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(node_cap, need);
  node_kind = (int*)grow_ints(node_kind, node_cap, n);
  node_a = (int*)grow_ints(node_a, node_cap, n);
  node_b = (int*)grow_ints(node_b, node_cap, n);
  node_c = (int*)grow_ints(node_c, node_cap, n);
  node_next = (int*)grow_ints(node_next, node_cap, n);
  node_value = (int*)grow_ints(node_value, node_cap, n);
  node_aux = (int*)grow_ints(node_aux, node_cap, n);
  node_pos = (int*)grow_ints(node_pos, node_cap, n);
  node_scope = (int*)grow_ints(node_scope, node_cap, n);
  node_cap = n;
}

void ensure_payload(int need) {
  if ((need < payload_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(payload_cap, need);
  payload_int = (int*)grow_ints(payload_int, payload_cap, n);
  payload_name = (int*)grow_ints(payload_name, payload_cap, n);
  payload_string = (int*)grow_ints(payload_string, payload_cap, n);
  payload_cap = n;
}

void ensure_code(int need) {
  if ((need < code_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(code_cap, need);
  code_kind = (int*)grow_ints(code_kind, code_cap, n);
  code_value = (int*)grow_ints(code_value, code_cap, n);
  code_cap = n;
}

void ensure_input(int need) {
  if ((need < input_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(input_cap, need);
  input_kind = (int*)grow_ints(input_kind, input_cap, n);
  input_value = (int*)grow_ints(input_value, input_cap, n);
  input_cap = n;
}

void ensure_source(int need) {
  if ((need < source_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(source_cap, need);
  source = (int*)grow_ints(source, source_cap, n);
  source_cap = n;
}

void ensure_sym(int need) {
  if ((need < sym_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(sym_cap, need);
  sym_start = (int*)grow_ints(sym_start, sym_cap, n);
  sym_len = (int*)grow_ints(sym_len, sym_cap, n);
  sym_hash = (int*)grow_ints(sym_hash, sym_cap, n);
  sym_kind = (int*)grow_ints(sym_kind, sym_cap, n);
  sym_type = (int*)grow_ints(sym_type, sym_cap, n);
  sym_elem_kind = (int*)grow_ints(sym_elem_kind, sym_cap, n);
  sym_elem_name = (int*)grow_ints(sym_elem_name, sym_cap, n);
  sym_scope = (int*)grow_ints(sym_scope, sym_cap, n);
  sym_cap = n;
}

int ast_node(int kind, int a, int b, int c, int value, int aux) {
  int id = node_count;
  ensure_node(id);
  (node_kind)[id] = kind;
  (node_a)[id] = a;
  (node_b)[id] = b;
  (node_c)[id] = c;
  (node_next)[id] = 0;
  (node_value)[id] = value;
  (node_aux)[id] = aux;
  (node_pos)[id] = current_source_pos;
  (node_scope)[id] = ast_namespace_scope;
  node_count = (node_count + 1);
  return id;
}

int ast_link(int head, int item) {
  if ((head == 0)) {
    return item;
  } else {
    {
    }
  }
  int p = head;
  while (((node_next)[p] != 0)) {
    {
      p = (node_next)[p];
    }
  }
  (node_next)[p] = item;
  return head;
}

int payload_make_int(int value) {
  int id = payload_count;
  ensure_payload(id);
  (payload_int)[id] = value;
  payload_count = (payload_count + 1);
  return id;
}

int payload_make_name(int name_id) {
  int id = payload_count;
  ensure_payload(id);
  (payload_name)[id] = name_id;
  payload_count = (payload_count + 1);
  return id;
}

int payload_make_string(int string_id) {
  int id = payload_count;
  ensure_payload(id);
  (payload_string)[id] = string_id;
  payload_count = (payload_count + 1);
  return id;
}

void gen_bind_clear(void) {
  gen_bind_count = 0;
}

void ensure_gen_bind(int need) {
  if ((need < gen_bind_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(gen_bind_cap, need);
  gen_bind_name = (int*)grow_ints(gen_bind_name, gen_bind_cap, n);
  gen_bind_type = (int*)grow_ints(gen_bind_type, gen_bind_cap, n);
  gen_bind_cap = n;
}

int gen_bind_find(int name) {
  int i = 0;
  while ((i < gen_bind_count)) {
    {
      if (((gen_bind_name)[i] == name)) {
        return (gen_bind_type)[i];
      } else {
        {
        }
      }
      i = (i + 1);
    }
  }
  return 0;
}

void gen_bind_add(int name, int ty) {
  int i = 0;
  while ((i < gen_bind_count)) {
    {
      if (((gen_bind_name)[i] == name)) {
        {
          (gen_bind_type)[i] = ty;
          return;
        }
      } else {
        {
        }
      }
      i = (i + 1);
    }
  }
  ensure_gen_bind(gen_bind_count);
  (gen_bind_name)[gen_bind_count] = name;
  (gen_bind_type)[gen_bind_count] = ty;
  gen_bind_count = (gen_bind_count + 1);
}

void gen_append_char(int c) {
  ensure_source(source_len);
  (source)[source_len] = c;
  source_len = (source_len + 1);
  gen_mangle_len = (gen_mangle_len + 1);
}

void gen_append_char_text(char c) {
  ensure_source(source_len);
  (source)[source_len] = c;
  source_len = (source_len + 1);
  gen_mangle_len = (gen_mangle_len + 1);
}

void gen_append_text(char* text) {
  int i = 0;
  while (((text)[i] != '\0')) {
    {
      gen_append_char_text((text)[i]);
      i = (i + 1);
    }
  }
}

void gen_append_symbol(int id) {
  int i = 0;
  while ((i < (sym_len)[id])) {
    {
      gen_append_char((source)[((sym_start)[id] + i)]);
      i = (i + 1);
    }
  }
}

void gen_append_c_symbol(int id) {
  int i = 0;
  while ((i < (sym_len)[id])) {
    {
      int c = (source)[((sym_start)[id] + i)];
      if ((((c == 58) && ((i + 1) < (sym_len)[id])) && ((source)[(((sym_start)[id] + i) + 1)] == 58))) {
        {
          gen_append_char(95);
          gen_append_char(95);
          i = (i + 2);
        }
      } else {
        {
          gen_append_char(c);
          i = (i + 1);
        }
      }
    }
  }
}

int sym_c_symbol(int id) {
  gen_mangle_start = source_len;
  gen_mangle_len = 0;
  gen_append_c_symbol(id);
  return sym_intern(gen_mangle_start, gen_mangle_len, L_ID, 0);
}

void gen_mangle_type(int ty) {
  if ((ty == 0)) {
    {
      gen_append_text("void");
      return;
    }
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_PARAM)) {
    {
      int b = gen_bind_find((node_value)[ty]);
      if ((b != 0)) {
        {
          gen_mangle_type(b);
          return;
        }
      } else {
        {
        }
      }
      gen_append_c_symbol((node_value)[ty]);
      return;
    }
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_INT)) {
    gen_append_text("int");
  } else {
    if (((node_kind)[ty] == TY_BOOL)) {
      gen_append_text("bool");
    } else {
      if (((node_kind)[ty] == TY_STRING)) {
        gen_append_text("char_ptr");
      } else {
        if (((node_kind)[ty] == TY_CHAR)) {
          gen_append_text("char");
        } else {
          if (((node_kind)[ty] == TY_FLOAT)) {
            gen_append_text("float");
          } else {
            if (((node_kind)[ty] == TY_DOUBLE)) {
              gen_append_text("double");
            } else {
              if (((node_kind)[ty] == TY_VOID)) {
                gen_append_text("void");
              } else {
                if (((node_kind)[ty] == TY_NAMED)) {
                  gen_append_c_symbol((node_value)[ty]);
                } else {
                  if (((node_kind)[ty] == TY_PTR)) {
                    {
                      gen_append_text("ptr_");
                      gen_mangle_type((node_a)[ty]);
                    }
                  } else {
                    if ((((node_kind)[ty] == TY_ARRAY) || ((node_kind)[ty] == TY_DYN_ARRAY))) {
                      {
                        gen_append_text("array_");
                        gen_mangle_type((node_a)[ty]);
                      }
                    } else {
                      if (((node_kind)[ty] == TY_GENERIC)) {
                        {
                          gen_append_c_symbol((node_value)[ty]);
                          gen_append_text("__");
                          int a = (node_a)[ty];
                          while ((a != 0)) {
                            {
                              gen_mangle_type(a);
                              if (((node_next)[a] != 0)) {
                                gen_append_text("__");
                              } else {
                                {
                                }
                              }
                              a = (node_next)[a];
                            }
                          }
                        }
                      } else {
                        gen_append_text("int");
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

int gen_mangled_type_symbol(int ty) {
  gen_mangle_start = source_len;
  gen_mangle_len = 0;
  gen_mangle_type(ty);
  return sym_intern(gen_mangle_start, gen_mangle_len, 0, 0);
}

int gen_mangled_function_symbol(int base, int args) {
  gen_mangle_start = source_len;
  gen_mangle_len = 0;
  gen_append_c_symbol(base);
  gen_append_text("__");
  int a = args;
  while ((a != 0)) {
    {
      gen_mangle_type(a);
      if (((node_next)[a] != 0)) {
        gen_append_text("__");
      } else {
        {
        }
      }
      a = (node_next)[a];
    }
  }
  return sym_intern(gen_mangle_start, gen_mangle_len, 0, 0);
}

void code_emit(int kind, int value) {
  ensure_code(code_count);
  (code_kind)[code_count] = kind;
  (code_value)[code_count] = value;
  code_count = (code_count + 1);
}

void code_reset(void) {
  code_count = 0;
}

void ensure_gen_specs(int need) {
  if ((need < gen_spec_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(gen_spec_cap, need);
  gen_spec_kind = (int*)grow_ints(gen_spec_kind, gen_spec_cap, n);
  gen_spec_decl = (int*)grow_ints(gen_spec_decl, gen_spec_cap, n);
  gen_spec_type = (int*)grow_ints(gen_spec_type, gen_spec_cap, n);
  gen_spec_name = (int*)grow_ints(gen_spec_name, gen_spec_cap, n);
  gen_spec_cap = n;
}

int gen_substitute_type(int ty) {
  if ((ty == 0)) {
    return 0;
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_PARAM)) {
    {
      int b = gen_bind_find((node_value)[ty]);
      if ((b != 0)) {
        return gen_substitute_type(b);
      } else {
        {
        }
      }
      return ast_node(TY_PARAM, (node_a)[ty], (node_b)[ty], (node_c)[ty], (node_value)[ty], (node_aux)[ty]);
    }
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_PTR)) {
    return ast_node(TY_PTR, gen_substitute_type((node_a)[ty]), 0, 0, 0, 0);
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_ARRAY)) {
    return ast_node(TY_ARRAY, gen_substitute_type((node_a)[ty]), 0, 0, (node_value)[ty], 0);
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_DYN_ARRAY)) {
    return ast_node(TY_DYN_ARRAY, gen_substitute_type((node_a)[ty]), 0, 0, 0, 0);
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_GENERIC)) {
    {
      int args = 0;
      int p = (node_a)[ty];
      while ((p != 0)) {
        {
          int q = gen_substitute_type(p);
          if ((args == 0)) {
            args = q;
          } else {
            args = ast_link(args, q);
          }
          p = (node_next)[p];
        }
      }
      return ast_node(TY_GENERIC, args, 0, 0, (node_value)[ty], 0);
    }
  } else {
    {
    }
  }
  return ast_node((node_kind)[ty], (node_a)[ty], (node_b)[ty], (node_c)[ty], (node_value)[ty], (node_aux)[ty]);
}

int gen_spec_exists(int kind, int decl, int name) {
  int i = 0;
  while ((i < gen_spec_count)) {
    {
      if (((((gen_spec_kind)[i] == kind) && ((gen_spec_decl)[i] == decl)) && ((gen_spec_name)[i] == name))) {
        return 1;
      } else {
        {
        }
      }
      i = (i + 1);
    }
  }
  return 0;
}

void gen_add_struct_spec(int ty) {
  int q = gen_substitute_type(ty);
  if (((q == 0) || ((node_kind)[q] != TY_GENERIC))) {
    return;
  } else {
    {
    }
  }
  int decl = tc_find_struct((node_value)[q]);
  if ((decl == 0)) {
    return;
  } else {
    {
    }
  }
  int name = gen_mangled_type_symbol(q);
  if ((gen_spec_exists(1, decl, name) == 1)) {
    return;
  } else {
    {
    }
  }
  ensure_gen_specs(gen_spec_count);
  (gen_spec_kind)[gen_spec_count] = 1;
  (gen_spec_decl)[gen_spec_count] = decl;
  (gen_spec_type)[gen_spec_count] = q;
  (gen_spec_name)[gen_spec_count] = name;
  gen_spec_count = (gen_spec_count + 1);
  int a = (node_a)[q];
  while ((a != 0)) {
    {
      gen_collect_type(a);
      a = (node_next)[a];
    }
  }
}

void gen_add_fun_spec(int decl, int args) {
  int actual = 0;
  int p = args;
  while ((p != 0)) {
    {
      int q = gen_substitute_type(p);
      if ((actual == 0)) {
        actual = q;
      } else {
        actual = ast_link(actual, q);
      }
      p = (node_next)[p];
    }
  }
  gen_bind_decl(decl, actual);
  int typeargs = 0;
  int tp = (node_aux)[decl];
  while ((tp != 0)) {
    {
      int bt = gen_bind_find((node_a)[tp]);
      if ((bt == 0)) {
        bt = ast_node(TY_PARAM, 0, 0, 0, (node_a)[tp], 0);
      } else {
        {
        }
      }
      int cq = gen_substitute_type(bt);
      if ((typeargs == 0)) {
        typeargs = cq;
      } else {
        typeargs = ast_link(typeargs, cq);
      }
      tp = (node_next)[tp];
    }
  }
  gen_bind_clear();
  int name = gen_mangled_function_symbol((node_value)[decl], typeargs);
  if ((gen_spec_exists(2, decl, name) == 1)) {
    return;
  } else {
    {
    }
  }
  ensure_gen_specs(gen_spec_count);
  (gen_spec_kind)[gen_spec_count] = 2;
  (gen_spec_decl)[gen_spec_count] = decl;
  (gen_spec_type)[gen_spec_count] = actual;
  (gen_spec_name)[gen_spec_count] = name;
  gen_spec_count = (gen_spec_count + 1);
  int a = actual;
  while ((a != 0)) {
    {
      gen_collect_type(a);
      a = (node_next)[a];
    }
  }
}

void gen_collect_type(int ty) {
  if ((ty == 0)) {
    {
      return;
    }
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_GENERIC)) {
    {
      gen_add_struct_spec(ty);
    }
  } else {
    if (((((node_kind)[ty] == TY_PTR) || ((node_kind)[ty] == TY_ARRAY)) || ((node_kind)[ty] == TY_DYN_ARRAY))) {
      {
        gen_collect_type((node_a)[ty]);
      }
    } else {
      {
      }
    }
  }
}

void gen_collect_expr(int id) {
  if ((id == 0)) {
    return;
  } else {
    {
    }
  }
  int k = (node_kind)[id];
  if ((k == N_CALL)) {
    {
      int f = tc_find_function_ctx((node_value)[id], (node_scope)[id]);
      if (((f != 0) && ((node_kind)[f] == N_GENERIC_FUNC))) {
        {
          int aa = (node_a)[id];
          int actual = 0;
          while ((aa != 0)) {
            {
              int q = 0;
              if ((((node_kind)[aa] == N_VAR) && ((node_aux)[aa] != 0))) {
                q = (node_aux)[aa];
              } else {
                if (((node_kind)[aa] == N_STRING)) {
                  q = ast_node(TY_STRING, 0, 0, 0, 0, 0);
                } else {
                  {
                    tc_expr(aa);
                    q = tc_result_type;
                    if ((q == 0)) {
                      q = tc_type_node_from_summary(tc_kind, tc_name, tc_elem_kind, tc_elem_name);
                    } else {
                      {
                      }
                    }
                  }
                }
              }
              if ((q != 0)) {
                q = gen_substitute_type(q);
              } else {
                {
                }
              }
              if ((actual == 0)) {
                actual = q;
              } else {
                actual = ast_link(actual, q);
              }
              gen_collect_type(q);
              aa = (node_next)[aa];
            }
          }
          gen_add_fun_spec(f, actual);
        }
      } else {
        {
          int aa = (node_a)[id];
          while ((aa != 0)) {
            {
              gen_collect_expr(aa);
              aa = (node_next)[aa];
            }
          }
        }
      }
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_BINOP)) {
    {
      gen_collect_expr((node_a)[id]);
      gen_collect_expr((node_b)[id]);
      return;
    }
  } else {
    {
    }
  }
  if (((((k == N_FIELD_ACCESS) || (k == N_INDEX)) || (k == N_DEREF)) || (k == N_ADDRESS))) {
    {
      gen_collect_expr((node_a)[id]);
      gen_collect_expr((node_b)[id]);
      return;
    }
  } else {
    {
    }
  }
}

void gen_collect_stmt(int id) {
  if ((id == 0)) {
    return;
  } else {
    {
    }
  }
  int k = (node_kind)[id];
  if (((k == N_LET) || (k == N_CONST))) {
    {
      gen_collect_type((node_b)[id]);
      gen_collect_expr((node_c)[id]);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_GLOBAL)) {
    {
      gen_collect_type((node_b)[id]);
      gen_collect_expr((node_c)[id]);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_ASSIGN)) {
    {
      gen_collect_expr((node_a)[id]);
      gen_collect_expr((node_b)[id]);
      return;
    }
  } else {
    {
    }
  }
  if ((((k == N_PRINT) || (k == N_EXPR)) || (k == N_RETURN))) {
    {
      gen_collect_expr((node_a)[id]);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_BLOCK)) {
    {
      int x = (node_a)[id];
      while ((x != 0)) {
        {
          gen_collect_stmt(x);
          x = (node_next)[x];
        }
      }
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_IF)) {
    {
      gen_collect_expr((node_a)[id]);
      gen_collect_stmt((node_b)[id]);
      gen_collect_stmt((node_c)[id]);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_WHILE)) {
    {
      gen_collect_expr((node_a)[id]);
      gen_collect_stmt((node_b)[id]);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_FOR)) {
    {
      gen_collect_stmt((node_a)[id]);
      gen_collect_expr((node_b)[id]);
      gen_collect_stmt((node_c)[id]);
      gen_collect_stmt((node_value)[id]);
      return;
    }
  } else {
    {
    }
  }
}

void gen_type(int kind, int child, int size) {
  if ((kind == TY_INT)) {
    code_emit(C_KW, 1);
  } else {
    if ((kind == TY_BOOL)) {
      code_emit(C_KW, 2);
    } else {
      if ((kind == TY_STRING)) {
        code_emit(C_KW, 3);
      } else {
        if ((kind == TY_CHAR)) {
          code_emit(C_KW, 17);
        } else {
          if ((kind == TY_FLOAT)) {
            code_emit(C_KW, 18);
          } else {
            if ((kind == TY_DOUBLE)) {
              code_emit(C_KW, 15);
            } else {
              if ((kind == TY_VOID)) {
                code_emit(C_KW, 4);
              } else {
                if ((kind == TY_PTR)) {
                  {
                    gen_type((node_kind)[(node_a)[child]], (node_a)[child], 0);
                    code_emit(C_PUNCT, 1);
                  }
                } else {
                  if ((kind == TY_ARRAY)) {
                    {
                      gen_type((node_kind)[child], (node_a)[child], (node_value)[child]);
                      code_emit(C_PUNCT, 2);
                      code_emit(C_INT, size);
                      code_emit(C_PUNCT, 3);
                    }
                  } else {
                    if ((kind == TY_DYN_ARRAY)) {
                      {
                        code_emit(C_IDENT, 1003);
                        code_emit(C_PUNCT, 18);
                      }
                    } else {
                      if ((kind == TY_NAMED)) {
                        {
                          code_emit(C_IDENT, sym_c_symbol((node_value)[child]));
                          code_emit(C_PUNCT, 18);
                        }
                      } else {
                        if ((kind == TY_GENERIC)) {
                          {
                            code_emit(C_IDENT, gen_mangled_type_symbol(child));
                            code_emit(C_PUNCT, 18);
                          }
                        } else {
                          if ((kind == TY_PARAM)) {
                            {
                              int b = gen_bind_find((node_value)[child]);
                              if ((b != 0)) {
                                gen_type((node_kind)[b], b, (node_value)[b]);
                              } else {
                                code_emit(C_IDENT, sym_c_symbol((node_value)[child]));
                              }
                              code_emit(C_PUNCT, 18);
                            }
                          } else {
                            {
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

int gen_array_elem_kind(int arg) {
  if (((arg != 0) && ((node_kind)[arg] == N_VAR))) {
    return (sym_elem_kind)[(node_value)[arg]];
  } else {
    {
    }
  }
  return TY_INT;
}

int gen_array_elem_name(int arg) {
  if (((arg != 0) && ((node_kind)[arg] == N_VAR))) {
    return (sym_elem_name)[(node_value)[arg]];
  } else {
    {
    }
  }
  return 0;
}

void gen_array_elem_type(int kind, int name) {
  if ((kind == TY_INT)) {
    code_emit(C_KW, 1);
  } else {
    if ((kind == TY_BOOL)) {
      code_emit(C_KW, 2);
    } else {
      if ((kind == TY_CHAR)) {
        code_emit(C_KW, 17);
      } else {
        if ((kind == TY_FLOAT)) {
          code_emit(C_KW, 18);
        } else {
          if ((kind == TY_DOUBLE)) {
            code_emit(C_KW, 15);
          } else {
            if ((kind == TY_STRING)) {
              {
                code_emit(C_KW, 17);
                code_emit(C_PUNCT, 1);
              }
            } else {
              if ((kind == TY_NAMED)) {
                code_emit(C_IDENT, name);
              } else {
                if ((kind == TY_PTR)) {
                  {
                    code_emit(C_KW, 1);
                    code_emit(C_PUNCT, 1);
                  }
                } else {
                  code_emit(C_KW, 1);
                }
              }
            }
          }
        }
      }
    }
  }
}

void gen_array_sizeof(int kind, int name) {
  code_emit(C_IDENT, 1011);
  code_emit(C_PUNCT, 4);
  gen_array_elem_type(kind, name);
  code_emit(C_PUNCT, 5);
}

void gen_array_value_ptr(int kind, int name, int value) {
  code_emit(C_PUNCT, 10);
  code_emit(C_PUNCT, 4);
  gen_array_elem_type(kind, name);
  if ((kind == TY_NAMED)) {
    {
      code_emit(C_PUNCT, 2);
      code_emit(C_PUNCT, 3);
    }
  } else {
    {
    }
  }
  code_emit(C_PUNCT, 5);
  code_emit(C_PUNCT, 24);
  if ((kind == TY_FLOAT)) {
    {
      code_emit(C_PUNCT, 4);
      code_emit(C_KW, 18);
      code_emit(C_PUNCT, 5);
    }
  } else {
    {
    }
  }
  gen_expr(value);
  code_emit(C_PUNCT, 25);
}

void gen_array_make_expr(int capacity, int kind, int name) {
  code_emit(C_IDENT, 1004);
  code_emit(C_PUNCT, 6);
  gen_expr(capacity);
  code_emit(C_PUNCT, 7);
  gen_array_sizeof(kind, name);
  code_emit(C_PUNCT, 8);
}

void gen_array_checked_get(int arr, int pos, int kind, int name) {
  code_emit(C_PUNCT, 9);
  code_emit(C_PUNCT, 6);
  code_emit(C_PUNCT, 6);
  gen_array_elem_type(kind, name);
  code_emit(C_PUNCT, 1);
  code_emit(C_PUNCT, 5);
  code_emit(C_IDENT, 1014);
  code_emit(C_PUNCT, 6);
  code_emit(C_PUNCT, 10);
  code_emit(C_PUNCT, 6);
  gen_expr(arr);
  code_emit(C_PUNCT, 5);
  code_emit(C_PUNCT, 7);
  gen_expr(pos);
  code_emit(C_PUNCT, 7);
  gen_array_sizeof(kind, name);
  code_emit(C_PUNCT, 8);
  code_emit(C_PUNCT, 5);
}

void gen_array_builtin(int id) {
  int call_name = (node_value)[id];
  int call_len = (sym_len)[call_name];
  int call_hash = (sym_hash)[call_name];
  int a = (node_a)[id];
  int ek = gen_array_elem_kind(a);
  int en = gen_array_elem_name(a);
  if (((call_len == 10) && (call_hash == 790299))) {
    {
      code_emit(C_IDENT, 1004);
      code_emit(C_PUNCT, 6);
      gen_expr(a);
      code_emit(C_PUNCT, 7);
      gen_array_sizeof(ek, en);
      code_emit(C_PUNCT, 8);
    }
  } else {
    if (((call_len == 13) && (call_hash == 333999))) {
      {
        code_emit(C_IDENT, 1008);
        code_emit(C_PUNCT, 6);
        code_emit(C_PUNCT, 10);
        gen_expr(a);
        code_emit(C_PUNCT, 7);
        gen_expr((node_next)[a]);
        code_emit(C_PUNCT, 7);
        gen_array_sizeof(ek, en);
        code_emit(C_PUNCT, 8);
      }
    } else {
      if (((call_len == 10) && (call_hash == 899143))) {
        {
          code_emit(C_IDENT, 1005);
          code_emit(C_PUNCT, 6);
          code_emit(C_PUNCT, 10);
          gen_expr(a);
          code_emit(C_PUNCT, 7);
          gen_array_value_ptr(ek, en, (node_next)[a]);
          code_emit(C_PUNCT, 7);
          gen_array_sizeof(ek, en);
          code_emit(C_PUNCT, 8);
        }
      } else {
        if (((call_len == 9) && (call_hash == 890825))) {
          {
            gen_array_checked_get(a, (node_next)[a], ek, en);
          }
        } else {
          if (((call_len == 9) && (call_hash == 902357))) {
            {
              code_emit(C_IDENT, 1007);
              code_emit(C_PUNCT, 6);
              code_emit(C_PUNCT, 10);
              gen_expr(a);
              code_emit(C_PUNCT, 7);
              gen_expr((node_next)[a]);
              code_emit(C_PUNCT, 7);
              gen_array_value_ptr(ek, en, (node_next)[(node_next)[a]]);
              code_emit(C_PUNCT, 7);
              gen_array_sizeof(ek, en);
              code_emit(C_PUNCT, 8);
            }
          } else {
            if (((call_len == 11) && (call_hash == 585984))) {
              {
                code_emit(C_IDENT, 1009);
                code_emit(C_PUNCT, 6);
                code_emit(C_PUNCT, 10);
                gen_expr(a);
                code_emit(C_PUNCT, 8);
              }
            } else {
              if (((call_len == 10) && (call_hash == 597913))) {
                {
                  code_emit(C_IDENT, 1010);
                  code_emit(C_PUNCT, 6);
                  code_emit(C_PUNCT, 10);
                  gen_expr(a);
                  code_emit(C_PUNCT, 8);
                }
              } else {
                {
                  code_emit(C_IDENT, sym_c_symbol(call_name));
                  code_emit(C_PUNCT, 6);
                  int arg = a;
                  while ((arg != 0)) {
                    {
                      gen_expr(arg);
                      if (((node_next)[arg] != 0)) {
                        code_emit(C_PUNCT, 7);
                      } else {
                        {
                        }
                      }
                      arg = (node_next)[arg];
                    }
                  }
                  code_emit(C_PUNCT, 8);
                }
              }
            }
          }
        }
      }
    }
  }
}

int gen_call_name(int id) {
  int f = tc_find_function_ctx((node_value)[id], (node_scope)[id]);
  if (((f == 0) || ((node_kind)[f] != N_GENERIC_FUNC))) {
    return sym_c_symbol((node_value)[id]);
  } else {
    {
    }
  }
  int actual = 0;
  int a = (node_a)[id];
  while ((a != 0)) {
    {
      int q = 0;
      if ((((node_kind)[a] == N_VAR) && ((node_aux)[a] != 0))) {
        q = (node_aux)[a];
      } else {
        if (((node_kind)[a] == N_STRING)) {
          q = ast_node(TY_STRING, 0, 0, 0, 0, 0);
        } else {
          {
            tc_expr(a);
            q = tc_result_type;
            if ((q == 0)) {
              q = tc_type_node_from_summary(tc_kind, tc_name, tc_elem_kind, tc_elem_name);
            } else {
              {
              }
            }
          }
        }
      }
      if ((q != 0)) {
        q = gen_substitute_type(q);
      } else {
        {
        }
      }
      if ((actual == 0)) {
        actual = q;
      } else {
        actual = ast_link(actual, q);
      }
      a = (node_next)[a];
    }
  }
  gen_bind_decl(f, actual);
  int typeargs = 0;
  int tp = (node_aux)[f];
  while ((tp != 0)) {
    {
      int bt = gen_bind_find((node_a)[tp]);
      if ((bt == 0)) {
        bt = ast_node(TY_PARAM, 0, 0, 0, (node_a)[tp], 0);
      } else {
        {
        }
      }
      int cq = gen_substitute_type(bt);
      if ((typeargs == 0)) {
        typeargs = cq;
      } else {
        typeargs = ast_link(typeargs, cq);
      }
      tp = (node_next)[tp];
    }
  }
  gen_bind_clear();
  return gen_mangled_function_symbol((node_value)[f], typeargs);
}

void gen_expr(int id) {
  int k = (node_kind)[id];
  if ((k == N_INT)) {
    code_emit(C_INT, (node_value)[id]);
  } else {
    if ((k == N_BOOL)) {
      code_emit(C_INT, (node_value)[id]);
    } else {
      if ((k == N_FLOAT)) {
        code_emit(C_IDENT, (node_value)[id]);
      } else {
        if ((k == N_STRING)) {
          code_emit(C_STRING, (node_value)[id]);
        } else {
          if ((k == N_CHAR)) {
            code_emit(C_INT, (node_value)[id]);
          } else {
            if ((k == N_NULL)) {
              {
                code_emit(C_INT, 0);
              }
            } else {
              if ((k == N_VAR)) {
                code_emit(C_IDENT, sym_c_symbol((node_value)[id]));
              } else {
                if ((k == N_BINOP)) {
                  {
                    if (((node_value)[id] == OP_CONCAT)) {
                      {
                        code_emit(C_IDENT, 1002);
                        code_emit(C_PUNCT, 6);
                        gen_expr((node_a)[id]);
                        code_emit(C_PUNCT, 7);
                        gen_expr((node_b)[id]);
                        code_emit(C_PUNCT, 8);
                      }
                    } else {
                      {
                        code_emit(C_PUNCT, 4);
                        gen_expr((node_a)[id]);
                        code_emit(C_OP, (node_value)[id]);
                        gen_expr((node_b)[id]);
                        code_emit(C_PUNCT, 5);
                      }
                    }
                  }
                } else {
                  if ((k == N_CALL)) {
                    {
                      int call_name = (node_value)[id];
                      int call_len = (sym_len)[call_name];
                      int call_hash = (sym_hash)[call_name];
                      if (((((((((call_len == 10) && (call_hash == 790299)) || ((call_len == 13) && (call_hash == 333999))) || ((call_len == 10) && (call_hash == 899143))) || ((call_len == 9) && (call_hash == 890825))) || ((call_len == 9) && (call_hash == 902357))) || ((call_len == 11) && (call_hash == 585984))) || ((call_len == 10) && (call_hash == 597913)))) {
                        gen_array_builtin(id);
                      } else {
                        {
                          code_emit(C_IDENT, gen_call_name(id));
                          code_emit(C_PUNCT, 6);
                          int arg = (node_a)[id];
                          while ((arg != 0)) {
                            {
                              gen_expr(arg);
                              if (((node_next)[arg] != 0)) {
                                code_emit(C_PUNCT, 7);
                              } else {
                                {
                                }
                              }
                              arg = (node_next)[arg];
                            }
                          }
                          code_emit(C_PUNCT, 8);
                        }
                      }
                    }
                  } else {
                    if ((k == N_DEREF)) {
                      {
                        code_emit(C_PUNCT, 9);
                        gen_expr((node_a)[id]);
                      }
                    } else {
                      if ((k == N_ADDRESS)) {
                        {
                          code_emit(C_PUNCT, 10);
                          gen_expr((node_a)[id]);
                        }
                      } else {
                        if ((k == N_INDEX)) {
                          {
                            int base_kind = gen_expr_kind((node_a)[id]);
                            if ((base_kind == TY_DYN_ARRAY)) {
                              gen_array_checked_get((node_a)[id], (node_b)[id], gen_array_elem_kind((node_a)[id]), gen_array_elem_name((node_a)[id]));
                            } else {
                              {
                                gen_expr((node_a)[id]);
                                code_emit(C_PUNCT, 2);
                                gen_expr((node_b)[id]);
                                code_emit(C_PUNCT, 3);
                              }
                            }
                          }
                        } else {
                          if ((k == N_FIELD_ACCESS)) {
                            {
                              gen_expr((node_a)[id]);
                              code_emit(C_PUNCT, 17);
                              code_emit(C_IDENT, (node_value)[id]);
                            }
                          } else {
                            {
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

int gen_expr_kind(int id) {
  int k = (node_kind)[id];
  if (((k == N_INT) || (k == N_BOOL))) {
    return TY_INT;
  } else {
    {
    }
  }
  if ((k == N_CHAR)) {
    return TY_CHAR;
  } else {
    {
    }
  }
  if ((k == N_FLOAT)) {
    return TY_DOUBLE;
  } else {
    {
    }
  }
  if ((k == N_STRING)) {
    return TY_STRING;
  } else {
    {
    }
  }
  if ((k == N_NULL)) {
    return TY_PTR;
  } else {
    {
    }
  }
  if ((k == N_VAR)) {
    {
      int vt = (sym_type)[(node_value)[id]];
      if ((vt > 99)) {
        {
          tc_elem_kind = (sym_elem_kind)[(node_value)[id]];
          tc_elem_name = (sym_elem_name)[(node_value)[id]];
          return (vt - 100);
        }
      } else {
        {
        }
      }
      return vt;
    }
  } else {
    {
    }
  }
  if ((k == N_INDEX)) {
    {
      int bt = gen_expr_kind((node_a)[id]);
      if ((bt == TY_STRING)) {
        return TY_CHAR;
      } else {
        {
        }
      }
      if ((bt == TY_PTR)) {
        return TY_INT;
      } else {
        {
        }
      }
      return TY_INT;
    }
  } else {
    {
    }
  }
  if ((k == N_DEREF)) {
    return TY_INT;
  } else {
    {
    }
  }
  if ((k == N_FIELD_ACCESS)) {
    return TY_INT;
  } else {
    {
    }
  }
  if ((k == N_CALL)) {
    {
      int call_name = (node_value)[id];
      if ((((sym_len)[call_name] == 9) && ((sym_hash)[call_name] == 890825))) {
        return gen_array_elem_kind((node_a)[id]);
      } else {
        {
        }
      }
      return tc_expr_kind_for_emit(id);
    }
  } else {
    {
    }
  }
  if ((k == N_BINOP)) {
    {
      if (((node_value)[id] == OP_CONCAT)) {
        return TY_STRING;
      } else {
        {
        }
      }
      if ((((((((node_value)[id] == OP_EQ) || ((node_value)[id] == OP_NEQ)) || ((node_value)[id] == OP_LT)) || ((node_value)[id] == OP_GT)) || ((node_value)[id] == OP_AND)) || ((node_value)[id] == OP_OR))) {
        return TY_BOOL;
      } else {
        {
        }
      }
      if (((((node_value)[id] == OP_SUB) && (gen_expr_kind((node_a)[id]) == TY_PTR)) && (gen_expr_kind((node_b)[id]) == TY_PTR))) {
        return TY_PTRDIFF;
      } else {
        {
        }
      }
      if (((((((node_value)[id] == OP_BITAND) || ((node_value)[id] == OP_BITOR)) || ((node_value)[id] == OP_BITXOR)) || ((node_value)[id] == OP_SHL)) || ((node_value)[id] == OP_SHR))) {
        return TY_INT;
      } else {
        {
        }
      }
      int ak = gen_expr_kind((node_a)[id]);
      int bk = gen_expr_kind((node_b)[id]);
      if (((ak == TY_DOUBLE) || (bk == TY_DOUBLE))) {
        return TY_DOUBLE;
      } else {
        {
        }
      }
      if (((ak == TY_FLOAT) || (bk == TY_FLOAT))) {
        return TY_FLOAT;
      } else {
        {
        }
      }
      if ((ak == TY_PTR)) {
        return TY_PTR;
      } else {
        {
        }
      }
      return TY_INT;
    }
  } else {
    {
    }
  }
  return TY_INT;
}

void gen_for_clause(int id) {
  if ((id == 0)) {
    return;
  } else {
    {
    }
  }
  if (((node_kind)[id] == N_LET)) {
    {
      gen_type((node_kind)[(node_b)[id]], (node_b)[id], (node_value)[(node_b)[id]]);
      code_emit(C_IDENT, (node_a)[id]);
      code_emit(C_PUNCT, 11);
      gen_expr((node_c)[id]);
    }
  } else {
    if (((node_kind)[id] == N_ASSIGN)) {
      {
        gen_expr((node_a)[id]);
        code_emit(C_PUNCT, 11);
        gen_expr((node_b)[id]);
      }
    } else {
      if (((node_kind)[id] == N_EXPR)) {
        gen_expr((node_a)[id]);
      } else {
        {
        }
      }
    }
  }
}

void gen_initializer(int ty, int expr) {
  int st = gen_substitute_type(ty);
  if ((((((node_kind)[st] == TY_NAMED) || ((node_kind)[st] == TY_GENERIC)) && ((node_kind)[expr] == N_INT)) && ((node_value)[expr] == 0))) {
    code_emit(C_PUNCT, 19);
  } else {
    if ((((((node_kind)[st] == TY_DYN_ARRAY) && ((node_kind)[expr] == N_CALL)) && ((sym_len)[(node_value)[expr]] == 10)) && ((sym_hash)[(node_value)[expr]] == 790299))) {
      {
        int elem = (node_a)[ty];
        gen_array_make_expr((node_a)[expr], (node_kind)[elem], (node_value)[elem]);
      }
    } else {
      gen_expr(expr);
    }
  }
}

void gen_fun_decl(int ty, int name) {
  int ret = (node_b)[ty];
  gen_type((node_kind)[ret], ret, (node_value)[ret]);
  code_emit(C_PUNCT, 4);
  code_emit(C_PUNCT, 9);
  code_emit(C_IDENT, name);
  code_emit(C_PUNCT, 5);
  code_emit(C_PUNCT, 6);
  int p = (node_a)[ty];
  while ((p != 0)) {
    {
      gen_type((node_kind)[p], p, (node_value)[p]);
      if (((node_next)[p] != 0)) {
        code_emit(C_PUNCT, 7);
      } else {
        {
        }
      }
      p = (node_next)[p];
    }
  }
  code_emit(C_PUNCT, 8);
}

void gen_decl(int ty, int name) {
  if (((node_kind)[ty] == TY_FUN)) {
    gen_fun_decl(ty, name);
  } else {
    if (((node_kind)[ty] == TY_ARRAY)) {
      {
        int inner = (node_a)[ty];
        gen_type((node_kind)[inner], inner, (node_value)[inner]);
        code_emit(C_IDENT, name);
        code_emit(C_PUNCT, 2);
        code_emit(C_INT, (node_value)[ty]);
        code_emit(C_PUNCT, 3);
      }
    } else {
      {
        gen_type((node_kind)[ty], ty, (node_value)[ty]);
        code_emit(C_IDENT, name);
      }
    }
  }
}

void gen_stmt(int id) {
  int k = (node_kind)[id];
  if ((k == N_GLOBAL)) {
    {
      gen_decl((node_b)[id], (node_a)[id]);
      code_emit(C_PUNCT, 11);
      gen_initializer((node_b)[id], (node_c)[id]);
      code_emit(C_PUNCT, 12);
      code_emit(C_NEWLINE, 0);
    }
  } else {
    if ((k == N_CONST)) {
      {
        code_emit(C_KW, 16);
        gen_decl((node_b)[id], (node_a)[id]);
        code_emit(C_PUNCT, 11);
        gen_initializer((node_b)[id], (node_c)[id]);
        code_emit(C_PUNCT, 12);
        code_emit(C_NEWLINE, 0);
      }
    } else {
      if ((k == N_LET)) {
        {
          gen_decl((node_b)[id], (node_a)[id]);
          code_emit(C_PUNCT, 11);
          gen_initializer((node_b)[id], (node_c)[id]);
          code_emit(C_PUNCT, 12);
          code_emit(C_NEWLINE, 0);
        }
      } else {
        if ((k == N_ASSIGN)) {
          {
            gen_expr((node_a)[id]);
            code_emit(C_PUNCT, 11);
            gen_expr((node_b)[id]);
            code_emit(C_PUNCT, 12);
            code_emit(C_NEWLINE, 0);
          }
        } else {
          if ((k == N_PRINT)) {
            {
              code_emit(C_IDENT, 1001);
              int pk = gen_expr_kind((node_a)[id]);
              if ((pk == TY_STRING)) {
                code_emit(C_PUNCT, 16);
              } else {
                if ((pk == TY_CHAR)) {
                  code_emit(C_PUNCT, 20);
                } else {
                  if ((pk == TY_PTRDIFF)) {
                    code_emit(C_PUNCT, 23);
                  } else {
                    if (((pk == TY_FLOAT) || (pk == TY_DOUBLE))) {
                      code_emit(C_PUNCT, 21);
                    } else {
                      code_emit(C_PUNCT, 15);
                    }
                  }
                }
              }
              gen_expr((node_a)[id]);
              code_emit(C_PUNCT, 8);
              code_emit(C_PUNCT, 12);
              code_emit(C_NEWLINE, 0);
            }
          } else {
            if ((k == N_EXPR)) {
              {
                gen_expr((node_a)[id]);
                code_emit(C_PUNCT, 12);
                code_emit(C_NEWLINE, 0);
              }
            } else {
              if ((k == N_RETURN)) {
                {
                  code_emit(C_KW, 5);
                  if (((node_a)[id] != 0)) {
                    gen_expr((node_a)[id]);
                  } else {
                    {
                    }
                  }
                  code_emit(C_PUNCT, 12);
                  code_emit(C_NEWLINE, 0);
                }
              } else {
                if ((k == N_BREAK)) {
                  {
                    code_emit(C_KW, 9);
                    code_emit(C_PUNCT, 12);
                    code_emit(C_NEWLINE, 0);
                  }
                } else {
                  if ((k == N_CONTINUE)) {
                    {
                      if ((emit_for_step != 0)) {
                        gen_stmt(emit_for_step);
                      } else {
                        {
                        }
                      }
                      code_emit(C_KW, 10);
                      code_emit(C_PUNCT, 12);
                      code_emit(C_NEWLINE, 0);
                    }
                  } else {
                    if ((k == N_BLOCK)) {
                      {
                        code_emit(C_PUNCT, 13);
                        int item = (node_a)[id];
                        while ((item != 0)) {
                          {
                            gen_stmt(item);
                            item = (node_next)[item];
                          }
                        }
                        code_emit(C_PUNCT, 14);
                      }
                    } else {
                      if ((k == N_IF)) {
                        {
                          code_emit(C_KW, 6);
                          code_emit(C_PUNCT, 6);
                          gen_expr((node_a)[id]);
                          code_emit(C_PUNCT, 8);
                          gen_stmt((node_b)[id]);
                          code_emit(C_KW, 7);
                          gen_stmt((node_c)[id]);
                        }
                      } else {
                        if ((k == N_FOR)) {
                          {
                            code_emit(C_KW, 11);
                            code_emit(C_PUNCT, 4);
                            gen_for_clause((node_a)[id]);
                            code_emit(C_PUNCT, 12);
                            gen_expr((node_b)[id]);
                            code_emit(C_PUNCT, 12);
                            gen_for_clause((node_value)[id]);
                            code_emit(C_PUNCT, 5);
                            gen_stmt((node_c)[id]);
                          }
                        } else {
                          if ((k == N_WHILE)) {
                            {
                              code_emit(C_KW, 8);
                              code_emit(C_PUNCT, 4);
                              gen_expr((node_a)[id]);
                              code_emit(C_PUNCT, 5);
                              int old_step = emit_for_step;
                              emit_for_step = (node_aux)[id];
                              gen_stmt((node_b)[id]);
                              emit_for_step = old_step;
                            }
                          } else {
                            {
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

void gen_unify_formal(int formal, int actual) {
  if (((formal == 0) || (actual == 0))) {
    return;
  } else {
    {
    }
  }
  if (((node_kind)[formal] == TY_PARAM)) {
    {
      if ((gen_bind_find((node_value)[formal]) == 0)) {
        gen_bind_add((node_value)[formal], actual);
      } else {
        {
        }
      }
      return;
    }
  } else {
    {
    }
  }
  if ((((node_kind)[formal] == TY_GENERIC) && ((node_kind)[actual] == TY_GENERIC))) {
    {
      if (((node_value)[formal] != (node_value)[actual])) {
        return;
      } else {
        {
        }
      }
      int fp = (node_a)[formal];
      int ap = (node_a)[actual];
      while (((fp != 0) && (ap != 0))) {
        {
          gen_unify_formal(fp, ap);
          fp = (node_next)[fp];
          ap = (node_next)[ap];
        }
      }
      return;
    }
  } else {
    {
    }
  }
  if ((((node_kind)[formal] == TY_PTR) && ((node_kind)[actual] == TY_PTR))) {
    {
      gen_unify_formal((node_a)[formal], (node_a)[actual]);
      return;
    }
  } else {
    {
    }
  }
  if ((((node_kind)[formal] == TY_ARRAY) && ((node_kind)[actual] == TY_ARRAY))) {
    {
      gen_unify_formal((node_a)[formal], (node_a)[actual]);
      return;
    }
  } else {
    {
    }
  }
  if ((((node_kind)[formal] == TY_DYN_ARRAY) && ((node_kind)[actual] == TY_DYN_ARRAY))) {
    {
      gen_unify_formal((node_a)[formal], (node_a)[actual]);
      return;
    }
  } else {
    {
    }
  }
}

void gen_bind_decl(int decl, int inst) {
  gen_bind_clear();
  if (((node_kind)[decl] == N_GENERIC_FUNC)) {
    {
      int p = (node_c)[decl];
      int a = inst;
      while (((p != 0) && (a != 0))) {
        {
          gen_unify_formal((node_b)[p], a);
          p = (node_next)[p];
          a = (node_next)[a];
        }
      }
      return;
    }
  } else {
    {
    }
  }
  int p = (node_c)[decl];
  int a = (node_a)[inst];
  while (((p != 0) && (a != 0))) {
    {
      gen_bind_add((node_a)[p], a);
      p = (node_next)[p];
      a = (node_next)[a];
    }
  }
}

void gen_struct_decl_specialized(int decl, int inst, int cname) {
  gen_bind_decl(decl, inst);
  code_emit(C_KW, 12);
  code_emit(C_IDENT, cname);
  code_emit(C_PUNCT, 13);
  int f = (node_a)[decl];
  while ((f != 0)) {
    {
      int ft = gen_substitute_type((node_b)[f]);
      gen_decl(ft, (node_a)[f]);
      code_emit(C_PUNCT, 12);
      code_emit(C_NEWLINE, 0);
      f = (node_next)[f];
    }
  }
  code_emit(C_PUNCT, 14);
  code_emit(C_PUNCT, 12);
  code_emit(C_NEWLINE, 0);
}

void gen_function_specialized(int decl, int inst, int cname) {
  gen_bind_decl(decl, inst);
  int ret = gen_substitute_type((node_b)[decl]);
  gen_type((node_kind)[ret], ret, (node_value)[ret]);
  code_emit(C_IDENT, cname);
  code_emit(C_PUNCT, 6);
  int p = (node_c)[decl];
  while ((p != 0)) {
    {
      int pt = gen_substitute_type((node_b)[p]);
      gen_decl(pt, (node_a)[p]);
      if (((node_next)[p] != 0)) {
        code_emit(C_PUNCT, 7);
      } else {
        {
        }
      }
      p = (node_next)[p];
    }
  }
  code_emit(C_PUNCT, 8);
  gen_stmt((node_a)[decl]);
  gen_bind_clear();
}

void gen_struct_decl(int id) {
  code_emit(C_KW, 12);
  code_emit(C_IDENT, (node_value)[id]);
  code_emit(C_PUNCT, 13);
  int f = (node_a)[id];
  while ((f != 0)) {
    {
      gen_decl((node_b)[f], (node_a)[f]);
      code_emit(C_PUNCT, 12);
      code_emit(C_NEWLINE, 0);
      f = (node_next)[f];
    }
  }
  code_emit(C_PUNCT, 14);
  code_emit(C_PUNCT, 12);
  code_emit(C_NEWLINE, 0);
}

void gen_enum_decl(int id) {
  code_emit(C_KW, 14);
  code_emit(C_KW, 13);
  code_emit(C_IDENT, (node_value)[id]);
  code_emit(C_PUNCT, 13);
  int f = (node_a)[id];
  while ((f != 0)) {
    {
      code_emit(C_IDENT, (node_a)[f]);
      code_emit(C_PUNCT, 11);
      code_emit(C_INT, (node_value)[f]);
      if (((node_next)[f] != 0)) {
        code_emit(C_PUNCT, 7);
      } else {
        {
        }
      }
      code_emit(C_NEWLINE, 0);
      f = (node_next)[f];
    }
  }
  code_emit(C_PUNCT, 14);
  code_emit(C_IDENT, (node_value)[id]);
  code_emit(C_PUNCT, 12);
  code_emit(C_NEWLINE, 0);
}

void gen_extern_param(int ty, int name) {
  if (((node_kind)[ty] == TY_STRING)) {
    {
      code_emit(C_KW, 16);
      code_emit(C_KW, 3);
      code_emit(C_IDENT, name);
    }
  } else {
    gen_decl(ty, name);
  }
}

void gen_function_signature(int id) {
  if ((((sym_len)[(node_value)[id]] == 4) && ((sym_hash)[(node_value)[id]] == 808448))) {
    code_emit(C_KW, 1);
  } else {
    gen_type((node_aux)[id], (node_b)[id], 0);
  }
  code_emit(C_IDENT, (node_value)[id]);
  code_emit(C_PUNCT, 6);
  int param = (node_c)[id];
  while ((param != 0)) {
    {
      if (((node_kind)[id] == N_EXTERN)) {
        gen_extern_param((node_b)[param], (node_a)[param]);
      } else {
        gen_decl((node_b)[param], (node_a)[param]);
      }
      if (((node_next)[param] != 0)) {
        code_emit(C_PUNCT, 7);
      } else {
        {
        }
      }
      param = (node_next)[param];
    }
  }
  code_emit(C_PUNCT, 8);
}

void gen_prototype(int id) {
  gen_function_signature(id);
  code_emit(C_PUNCT, 12);
  code_emit(C_NEWLINE, 0);
}

void gen_function(int id) {
  gen_function_signature(id);
  if (((((sym_len)[(node_value)[id]] == 4) && ((sym_hash)[(node_value)[id]] == 808448)) && ((node_kind)[(node_a)[id]] == N_BLOCK))) {
    {
      code_emit(C_PUNCT, 13);
      int item = (node_a)[(node_a)[id]];
      while ((item != 0)) {
        {
          gen_stmt(item);
          item = (node_next)[item];
        }
      }
      code_emit(C_KW, 5);
      code_emit(C_INT, 0);
      code_emit(C_PUNCT, 12);
      code_emit(C_NEWLINE, 0);
      code_emit(C_PUNCT, 14);
    }
  } else {
    gen_stmt((node_a)[id]);
  }
}

void gen_program(int id) {
  int gen_saved_node_count = node_count;
  code_reset();
  gen_spec_count = 0;
  gen_name_override = 0;
  int item = (node_a)[id];
  while ((item != 0)) {
    {
      if ((((node_kind)[item] == N_GLOBAL) || ((node_kind)[item] == N_CONST))) {
        gen_collect_stmt(item);
      } else {
        if (((node_kind)[item] == N_FUNC)) {
          {
            gen_collect_type((node_b)[item]);
            int pp = (node_c)[item];
            while ((pp != 0)) {
              {
                gen_collect_type((node_b)[pp]);
                pp = (node_next)[pp];
              }
            }
            gen_collect_stmt((node_a)[item]);
          }
        } else {
          {
          }
        }
      }
      item = (node_next)[item];
    }
  }
  item = (node_a)[id];
  while ((item != 0)) {
    {
      if (((node_kind)[item] == N_STRUCT)) {
        {
          code_emit(C_KW, 14);
          code_emit(C_KW, 12);
          code_emit(C_IDENT, (node_value)[item]);
          code_emit(C_PUNCT, 22);
          code_emit(C_IDENT, (node_value)[item]);
          code_emit(C_PUNCT, 12);
          code_emit(C_NEWLINE, 0);
        }
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  int si = 0;
  while ((si < gen_spec_count)) {
    {
      if (((gen_spec_kind)[si] == 1)) {
        {
          code_emit(C_KW, 14);
          code_emit(C_KW, 12);
          code_emit(C_IDENT, (gen_spec_name)[si]);
          code_emit(C_PUNCT, 22);
          code_emit(C_IDENT, (gen_spec_name)[si]);
          code_emit(C_PUNCT, 12);
          code_emit(C_NEWLINE, 0);
        }
      } else {
        {
        }
      }
      si = (si + 1);
    }
  }
  item = (node_a)[id];
  while ((item != 0)) {
    {
      if (((node_kind)[item] == N_STRUCT)) {
        gen_struct_decl(item);
      } else {
        if (((node_kind)[item] == N_ENUM)) {
          gen_enum_decl(item);
        } else {
          {
          }
        }
      }
      item = (node_next)[item];
    }
  }
  si = 0;
  while ((si < gen_spec_count)) {
    {
      if (((gen_spec_kind)[si] == 1)) {
        gen_struct_decl_specialized((gen_spec_decl)[si], (gen_spec_type)[si], (gen_spec_name)[si]);
      } else {
        {
        }
      }
      si = (si + 1);
    }
  }
  item = (node_a)[id];
  while ((item != 0)) {
    {
      if ((((node_kind)[item] == N_FUNC) || ((node_kind)[item] == N_EXTERN))) {
        gen_prototype(item);
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  si = 0;
  while ((si < gen_spec_count)) {
    {
      if (((gen_spec_kind)[si] == 2)) {
        {
          gen_bind_decl((gen_spec_decl)[si], (gen_spec_type)[si]);
          int rr = gen_substitute_type((node_b)[(gen_spec_decl)[si]]);
          gen_type((node_kind)[rr], rr, (node_value)[rr]);
          code_emit(C_IDENT, (gen_spec_name)[si]);
          code_emit(C_PUNCT, 6);
          int pp = (node_c)[(gen_spec_decl)[si]];
          while ((pp != 0)) {
            {
              int pt = gen_substitute_type((node_b)[pp]);
              gen_decl(pt, (node_a)[pp]);
              if (((node_next)[pp] != 0)) {
                code_emit(C_PUNCT, 7);
              } else {
                {
                }
              }
              pp = (node_next)[pp];
            }
          }
          code_emit(C_PUNCT, 8);
          code_emit(C_PUNCT, 12);
          code_emit(C_NEWLINE, 0);
          gen_bind_clear();
        }
      } else {
        {
        }
      }
      si = (si + 1);
    }
  }
  item = (node_a)[id];
  while ((item != 0)) {
    {
      if ((((node_kind)[item] == N_GLOBAL) || ((node_kind)[item] == N_CONST))) {
        gen_stmt(item);
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  si = 0;
  while ((si < gen_spec_count)) {
    {
      if (((gen_spec_kind)[si] == 2)) {
        gen_function_specialized((gen_spec_decl)[si], (gen_spec_type)[si], (gen_spec_name)[si]);
      } else {
        {
        }
      }
      si = (si + 1);
    }
  }
  item = (node_a)[id];
  while ((item != 0)) {
    {
      if (((node_kind)[item] == N_FUNC)) {
        gen_function(item);
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  node_count = gen_saved_node_count;
}

int build_regression_ast(void) {
  node_count = 1;
  payload_count = 1;
  int two = ast_node(N_INT, 0, 0, 0, 2, 0);
  int three = ast_node(N_INT, 0, 0, 0, 3, 0);
  int sum = ast_node(N_BINOP, two, three, 0, OP_ADD, 0);
  int print_stmt = ast_node(N_PRINT, sum, 0, 0, 0, 0);
  int zero = ast_node(N_INT, 0, 0, 0, 0, 0);
  int ret = ast_node(N_RETURN, zero, 0, 0, 0, 0);
  int body_items = ast_link(print_stmt, ret);
  int body = ast_node(N_BLOCK, body_items, 0, 0, 0, 0);
  int main_fn = ast_node(N_FUNC, body, 0, 0, 1, TY_INT);
  return ast_node(N_PROGRAM, main_fn, 0, 0, 0, 0);
}

void generator_regression_main(void) {
  int program = build_regression_ast();
  gen_program(program);
  int first_count = code_count;
  int* first_kind = (int*)alloc_ints(65536);
  int* first_value = (int*)alloc_ints(65536);
  int i = 0;
  while ((i < first_count)) {
    {
      (first_kind)[i] = (code_kind)[i];
      (first_value)[i] = (code_value)[i];
      i = (i + 1);
    }
  }
  gen_program(program);
  int same = 1;
  i = 0;
  if ((code_count != first_count)) {
    same = 0;
  } else {
    {
    }
  }
  while ((i < first_count)) {
    {
      if (((code_kind)[i] != (first_kind)[i])) {
        same = 0;
      } else {
        {
        }
      }
      if (((code_value)[i] != (first_value)[i])) {
        same = 0;
      } else {
        {
        }
      }
      i = (i + 1);
    }
  }
  if ((same == 1)) {
    printf("%d\n", 1);
  } else {
    printf("%d\n", 0);
  }
}

void input_reset(void) {
  input_count = 0;
  input_pos = 0;
}

void input_put(int kind, int value) {
  ensure_input(input_count);
  (input_kind)[input_count] = kind;
  (input_value)[input_count] = value;
  input_count = (input_count + 1);
}

int input_peek(void) {
  if ((input_pos < input_count)) {
    return (input_kind)[input_pos];
  } else {
    return T_EOF;
  }
}

int input_payload(void) {
  if ((input_pos < input_count)) {
    return (input_value)[input_pos];
  } else {
    return 0;
  }
}

int input_take(int kind) {
  if ((input_peek() == kind)) {
    {
      input_pos = (input_pos + 1);
      return 1;
    }
  } else {
    return 0;
  }
}

int ast_generic_param(int name) {
  int p = ast_generic_scope;
  while ((p != 0)) {
    {
      if (((node_a)[p] == name)) {
        return 1;
      } else {
        {
        }
      }
      p = (node_next)[p];
    }
  }
  return 0;
}

int ast_generic_params(void) {
  int params = 0;
  if ((input_take(T_LT) == 0)) {
    return 0;
  } else {
    {
    }
  }
  while ((1 == 1)) {
    {
      if ((input_peek() != T_ID)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int name = input_payload();
      input_pos = (input_pos + 1);
      int p = ast_node(N_PARAM, name, 0, 0, 0, 0);
      if ((params == 0)) {
        params = p;
      } else {
        params = ast_link(params, p);
      }
      if ((input_take(T_COMMA) == 1)) {
        {
        }
      } else {
        break;
      }
    }
  }
  if ((input_take(T_GT) == 0)) {
    return (0 - 1);
  } else {
    {
    }
  }
  return params;
}

int ast_type(void) {
  int base = 0;
  int named = 0;
  int ty = 0;
  if ((input_take(T_FN) == 1)) {
    {
      if ((input_take(T_LPAREN) == 0)) {
        return 0;
      } else {
        {
        }
      }
      int args = 0;
      if ((input_peek() != T_RPAREN)) {
        {
          int at = ast_type();
          if ((at == 0)) {
            return 0;
          } else {
            {
            }
          }
          args = at;
          while ((input_take(T_COMMA) == 1)) {
            {
              at = ast_type();
              if ((at == 0)) {
                return 0;
              } else {
                {
                }
              }
              args = ast_link(args, at);
            }
          }
        }
      } else {
        {
        }
      }
      if ((input_take(T_RPAREN) == 0)) {
        return 0;
      } else {
        {
        }
      }
      if ((input_take(T_COLON) == 0)) {
        return 0;
      } else {
        {
        }
      }
      int ret = ast_type();
      if ((ret == 0)) {
        return 0;
      } else {
        {
        }
      }
      return ast_node(TY_FUN, args, ret, 0, 0, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_ARRAY) == 1)) {
    {
      if ((input_take(T_LT) == 0)) {
        return 0;
      } else {
        {
        }
      }
      int elem = ast_type();
      if ((elem == 0)) {
        return 0;
      } else {
        {
        }
      }
      if ((input_take(T_GT) == 0)) {
        return 0;
      } else {
        {
        }
      }
      ty = ast_node(TY_DYN_ARRAY, elem, 0, 0, 0, 0);
    }
  } else {
    {
      if ((input_take(T_TINT) == 1)) {
        base = TY_INT;
      } else {
        if ((input_take(T_TBOOL) == 1)) {
          base = TY_BOOL;
        } else {
          if ((input_take(T_TSTRING) == 1)) {
            base = TY_STRING;
          } else {
            if ((input_take(T_TCHAR) == 1)) {
              base = TY_CHAR;
            } else {
              if ((input_take(T_FLOAT) == 1)) {
                base = TY_FLOAT;
              } else {
                if ((input_take(T_TDOUBLE) == 1)) {
                  base = TY_DOUBLE;
                } else {
                  if ((input_take(T_TVOID) == 1)) {
                    base = TY_VOID;
                  } else {
                    if ((input_peek() == T_ID)) {
                      {
                        named = input_payload();
                        input_pos = (input_pos + 1);
                        if ((input_take(T_SCOPE) == 1)) {
                          {
                            if ((input_peek() != T_ID)) {
                              return 0;
                            } else {
                              {
                              }
                            }
                            int rhs = input_payload();
                            input_pos = (input_pos + 1);
                            named = sym_qualified(named, rhs);
                          }
                        } else {
                          named = ast_type_name(named);
                        }
                        if ((input_peek() == T_LT)) {
                          {
                            int args = 0;
                            input_pos = (input_pos + 1);
                            if ((input_peek() != T_GT)) {
                              {
                                int at = ast_type();
                                if ((at == 0)) {
                                  return 0;
                                } else {
                                  {
                                  }
                                }
                                args = at;
                                while ((input_take(T_COMMA) == 1)) {
                                  {
                                    at = ast_type();
                                    if ((at == 0)) {
                                      return 0;
                                    } else {
                                      {
                                      }
                                    }
                                    args = ast_link(args, at);
                                  }
                                }
                              }
                            } else {
                              {
                              }
                            }
                            if ((input_take(T_GT) == 0)) {
                              return 0;
                            } else {
                              {
                              }
                            }
                            ty = ast_node(TY_GENERIC, args, 0, 0, named, 0);
                          }
                        } else {
                          if ((ast_generic_param(named) == 1)) {
                            ty = ast_node(TY_PARAM, 0, 0, 0, named, 0);
                          } else {
                            ty = ast_node(TY_NAMED, 0, 0, 0, named, 0);
                          }
                        }
                      }
                    } else {
                      return 0;
                    }
                  }
                }
              }
            }
          }
        }
      }
      if ((ty == 0)) {
        ty = ast_node(base, 0, 0, 0, named, 0);
      } else {
        {
        }
      }
    }
  }
  while ((input_take(T_STAR) == 1)) {
    {
      ty = ast_node(TY_PTR, ty, 0, 0, 0, 0);
    }
  }
  while ((input_take(T_LBRACK) == 1)) {
    {
      if ((input_peek() != T_INT)) {
        return 0;
      } else {
        {
        }
      }
      int size = input_payload();
      input_pos = (input_pos + 1);
      if ((input_take(T_RBRACK) == 0)) {
        return 0;
      } else {
        {
        }
      }
      ty = ast_node(TY_ARRAY, ty, 0, 0, size, 0);
    }
  }
  return ty;
}

int ast_primary(void) {
  if ((input_peek() == T_CHAR)) {
    {
      int value = input_payload();
      input_pos = (input_pos + 1);
      return ast_node(N_CHAR, 0, 0, 0, value, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_NULL) == 1)) {
    return ast_node(N_NULL, 0, 0, 0, 0, 0);
  } else {
    {
    }
  }
  if ((input_peek() == T_FLOAT)) {
    {
      int value = input_payload();
      input_pos = (input_pos + 1);
      return ast_node(N_FLOAT, 0, 0, 0, value, 0);
    }
  } else {
    {
    }
  }
  if ((input_peek() == T_INT)) {
    {
      int value = input_payload();
      input_pos = (input_pos + 1);
      return ast_node(N_INT, 0, 0, 0, value, 0);
    }
  } else {
    {
    }
  }
  if ((input_peek() == T_STRING)) {
    {
      int value = input_payload();
      input_pos = (input_pos + 1);
      return ast_node(N_STRING, 0, 0, 0, value, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_TRUE) == 1)) {
    return ast_node(N_BOOL, 0, 0, 0, 1, 0);
  } else {
    {
    }
  }
  if ((input_take(T_FALSE) == 1)) {
    return ast_node(N_BOOL, 0, 0, 0, 0, 0);
  } else {
    {
    }
  }
  if ((input_peek() == T_ID)) {
    {
      int name = input_payload();
      input_pos = (input_pos + 1);
      while ((input_take(T_SCOPE) == 1)) {
        {
          if ((input_peek() != T_ID)) {
            return (0 - 1);
          } else {
            {
            }
          }
          int rhs = input_payload();
          input_pos = (input_pos + 1);
          name = sym_qualified(name, rhs);
        }
      }
      if ((input_take(T_LPAREN) == 1)) {
        {
          int args = 0;
          if ((input_peek() != T_RPAREN)) {
            {
              int arg = ast_expr();
              if ((arg < 0)) {
                return (0 - 1);
              } else {
                {
                }
              }
              args = arg;
              while ((input_take(T_COMMA) == 1)) {
                {
                  arg = ast_expr();
                  if ((arg < 0)) {
                    return (0 - 1);
                  } else {
                    {
                    }
                  }
                  args = ast_link(args, arg);
                }
              }
            }
          } else {
            {
            }
          }
          if ((input_take(T_RPAREN) == 0)) {
            return (0 - 1);
          } else {
            {
            }
          }
          return ast_node(N_CALL, args, 0, 0, name, 0);
        }
      } else {
        {
        }
      }
      int base = ast_node(N_VAR, 0, 0, 0, name, 0);
      if ((input_take(T_DOT) == 1)) {
        {
          if ((input_peek() != T_ID)) {
            return (0 - 1);
          } else {
            {
            }
          }
          int field = input_payload();
          input_pos = (input_pos + 1);
          return ast_node(N_FIELD_ACCESS, base, 0, 0, field, 0);
        }
      } else {
        {
        }
      }
      return base;
    }
  } else {
    {
    }
  }
  if ((input_take(T_LPAREN) == 1)) {
    {
      int e = ast_expr();
      if ((e < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_RPAREN) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return e;
    }
  } else {
    {
    }
  }
  return (0 - 1);
}

int ast_unary(void) {
  if ((input_take(T_BITNOT) == 1)) {
    {
      int e = ast_unary();
      if ((e < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int allbits = ast_node(N_INT, 0, 0, 0, (0 - 1), 0);
      return ast_node(N_BINOP, e, allbits, 0, OP_BITXOR, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_STAR) == 1)) {
    {
      int e = ast_unary();
      if ((e < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return ast_node(N_DEREF, e, 0, 0, 0, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_AMP) == 1)) {
    {
      int e = ast_unary();
      if ((e < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return ast_node(N_ADDRESS, e, 0, 0, 0, 0);
    }
  } else {
    {
    }
  }
  return ast_primary();
}

int ast_precedence(int kind) {
  if ((kind == T_OR_OR)) {
    return 1;
  } else {
    {
    }
  }
  if ((kind == T_AND_AND)) {
    return 2;
  } else {
    {
    }
  }
  if ((kind == T_EQEQ)) {
    return 3;
  } else {
    {
    }
  }
  if ((kind == T_NEQ)) {
    return 3;
  } else {
    {
    }
  }
  if ((kind == T_LT)) {
    return 3;
  } else {
    {
    }
  }
  if ((kind == T_GT)) {
    return 3;
  } else {
    {
    }
  }
  if ((kind == T_CONCAT)) {
    return 4;
  } else {
    {
    }
  }
  if ((kind == T_BITOR)) {
    return 5;
  } else {
    {
    }
  }
  if ((kind == T_BITXOR)) {
    return 6;
  } else {
    {
    }
  }
  if ((kind == T_AMP)) {
    return 7;
  } else {
    {
    }
  }
  if ((kind == T_SHL)) {
    return 8;
  } else {
    {
    }
  }
  if ((kind == T_SHR)) {
    return 8;
  } else {
    {
    }
  }
  if ((kind == T_PLUS)) {
    return 9;
  } else {
    {
    }
  }
  if ((kind == T_MINUS)) {
    return 9;
  } else {
    {
    }
  }
  if ((kind == T_STAR)) {
    return 10;
  } else {
    {
    }
  }
  if ((kind == T_DIVIDE)) {
    return 10;
  } else {
    {
    }
  }
  if ((kind == T_MOD)) {
    return 10;
  } else {
    {
    }
  }
  return 0;
}

int ast_operator(int kind) {
  if ((kind == T_PLUS)) {
    return OP_ADD;
  } else {
    {
    }
  }
  if ((kind == T_MINUS)) {
    return OP_SUB;
  } else {
    {
    }
  }
  if ((kind == T_STAR)) {
    return OP_MUL;
  } else {
    {
    }
  }
  if ((kind == T_DIVIDE)) {
    return OP_DIV;
  } else {
    {
    }
  }
  if ((kind == T_MOD)) {
    return OP_MOD;
  } else {
    {
    }
  }
  if ((kind == T_EQEQ)) {
    return OP_EQ;
  } else {
    {
    }
  }
  if ((kind == T_NEQ)) {
    return OP_NEQ;
  } else {
    {
    }
  }
  if ((kind == T_LT)) {
    return OP_LT;
  } else {
    {
    }
  }
  if ((kind == T_GT)) {
    return OP_GT;
  } else {
    {
    }
  }
  if ((kind == T_AND_AND)) {
    return OP_AND;
  } else {
    {
    }
  }
  if ((kind == T_OR_OR)) {
    return OP_OR;
  } else {
    {
    }
  }
  if ((kind == T_CONCAT)) {
    return OP_CONCAT;
  } else {
    {
    }
  }
  if ((kind == T_AMP)) {
    return OP_BITAND;
  } else {
    {
    }
  }
  if ((kind == T_BITOR)) {
    return OP_BITOR;
  } else {
    {
    }
  }
  if ((kind == T_BITXOR)) {
    return OP_BITXOR;
  } else {
    {
    }
  }
  if ((kind == T_SHL)) {
    return OP_SHL;
  } else {
    {
    }
  }
  if ((kind == T_SHR)) {
    return OP_SHR;
  } else {
    {
    }
  }
  return OP_GT;
}

int ast_expr_prec(int min_prec) {
  int left = ast_unary();
  if ((left < 0)) {
    return (0 - 1);
  } else {
    {
    }
  }
  while ((1 == 1)) {
    {
      if ((input_take(T_LBRACK) == 1)) {
        {
          int index = ast_expr();
          if ((index < 0)) {
            return (0 - 1);
          } else {
            {
            }
          }
          if ((input_take(T_RBRACK) == 0)) {
            return (0 - 1);
          } else {
            {
            }
          }
          left = ast_node(N_INDEX, left, index, 0, 0, 0);
        }
      } else {
        {
          int p = ast_precedence(input_peek());
          if ((p < min_prec)) {
            return left;
          } else {
            {
            }
          }
          int op_token = input_peek();
          input_pos = (input_pos + 1);
          int right = ast_expr_prec((p + 1));
          if ((right < 0)) {
            return (0 - 1);
          } else {
            {
            }
          }
          left = ast_node(N_BINOP, left, right, 0, ast_operator(op_token), 0);
        }
      }
    }
  }
  return left;
}

int ast_expr(void) {
  return ast_expr_prec(1);
}

int clone_for_step(int step) {
  return ast_node((node_kind)[step], (node_a)[step], (node_b)[step], (node_c)[step], (node_value)[step], (node_aux)[step]);
}

int lower_for_stmt(int id, int step) {
  if (((node_kind)[id] == N_CONTINUE)) {
    {
      int s = clone_for_step(step);
      int c = ast_node(N_CONTINUE, 0, 0, 0, 0, 0);
      return ast_node(N_BLOCK, ast_link(s, c), 0, 0, 0, 0);
    }
  } else {
    {
    }
  }
  if (((node_kind)[id] == N_IF)) {
    {
      int yes = lower_for_stmt((node_b)[id], step);
      int no = lower_for_stmt((node_c)[id], step);
      return ast_node(N_IF, (node_a)[id], yes, no, (node_value)[id], (node_aux)[id]);
    }
  } else {
    {
    }
  }
  if (((node_kind)[id] == N_BLOCK)) {
    {
      int item = (node_a)[id];
      int out = 0;
      while ((item != 0)) {
        {
          int x = lower_for_stmt(item, step);
          if ((out == 0)) {
            out = x;
          } else {
            out = ast_link(out, x);
          }
          item = (node_next)[item];
        }
      }
      return ast_node(N_BLOCK, out, 0, 0, 0, 0);
    }
  } else {
    {
    }
  }
  return id;
}

int ast_stmt(void) {
  if ((input_take(T_BREAK) == 1)) {
    {
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return ast_node(N_BREAK, 0, 0, 0, 0, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_CONTINUE) == 1)) {
    {
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((for_step_context != 0)) {
        {
          int s = clone_for_step(for_step_context);
          int c = ast_node(N_CONTINUE, 0, 0, 0, 0, 0);
          return ast_node(N_BLOCK, ast_link(s, c), 0, 0, 0, 0);
        }
      } else {
        {
        }
      }
      return ast_node(N_CONTINUE, 0, 0, 0, 0, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_LET) == 1)) {
    {
      if ((input_peek() != T_ID)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int name = input_payload();
      input_pos = (input_pos + 1);
      if ((input_take(T_COLON) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int ty = ast_type();
      if ((ty == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_EQUAL) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int value = ast_expr();
      if ((value < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return ast_node(N_LET, name, ty, value, 0, (node_kind)[ty]);
    }
  } else {
    {
    }
  }
  if ((input_take(T_PRINT) == 1)) {
    {
      int value = ast_expr();
      if ((value < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return ast_node(N_PRINT, value, 0, 0, 0, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_IF) == 1)) {
    {
      int cond = ast_expr();
      if ((cond < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_THEN) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int yes = ast_stmt();
      if ((yes < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int no = 0;
      if ((input_take(T_ELSE) == 1)) {
        {
          no = ast_stmt();
          if ((no < 0)) {
            return (0 - 1);
          } else {
            {
            }
          }
        }
      } else {
        {
          no = ast_node(N_BLOCK, 0, 0, 0, 0, 0);
        }
      }
      return ast_node(N_IF, cond, yes, no, 0, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_FOR) == 1)) {
    {
      if ((input_take(T_LPAREN) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int init = 0;
      if ((input_peek() != T_SEMI)) {
        {
          if ((input_take(T_LET) == 1)) {
            {
              if ((input_peek() != T_ID)) {
                return (0 - 1);
              } else {
                {
                }
              }
              int n = input_payload();
              input_pos = (input_pos + 1);
              if ((input_take(T_COLON) == 0)) {
                return (0 - 1);
              } else {
                {
                }
              }
              int t = ast_type();
              if ((t == 0)) {
                return (0 - 1);
              } else {
                {
                }
              }
              if ((input_take(T_EQUAL) == 0)) {
                return (0 - 1);
              } else {
                {
                }
              }
              int v = ast_expr();
              init = ast_node(N_LET, n, t, v, 0, (node_kind)[t]);
            }
          } else {
            {
              int l = ast_expr();
              if ((l < 0)) {
                return (0 - 1);
              } else {
                {
                }
              }
              if ((input_take(T_EQUAL) == 0)) {
                return (0 - 1);
              } else {
                {
                }
              }
              int r = ast_expr();
              init = ast_node(N_ASSIGN, l, r, 0, 0, 0);
            }
          }
        }
      } else {
        {
        }
      }
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int cond = ast_expr();
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int step = 0;
      if ((input_peek() != T_RPAREN)) {
        {
          int l = ast_expr();
          if ((l < 0)) {
            return (0 - 1);
          } else {
            {
            }
          }
          if ((input_take(T_EQUAL) == 1)) {
            {
              int r = ast_expr();
              step = ast_node(N_ASSIGN, l, r, 0, 0, 0);
            }
          } else {
            step = ast_node(N_EXPR, l, 0, 0, 0, 0);
          }
        }
      } else {
        step = ast_node(N_BLOCK, 0, 0, 0, 0, 0);
      }
      if ((input_take(T_RPAREN) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_LBRACE) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      for_step_context = step;
      int items = 0;
      while ((input_peek() != T_RBRACE)) {
        {
          if ((input_peek() == T_EOF)) {
            return (0 - 1);
          } else {
            {
            }
          }
          int x = ast_stmt();
          if ((x < 0)) {
            return (0 - 1);
          } else {
            {
            }
          }
          if ((items == 0)) {
            items = x;
          } else {
            items = ast_link(items, x);
          }
        }
      }
      if ((input_take(T_RBRACE) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      for_step_context = 0;
      int body = ast_node(N_BLOCK, items, 0, 0, 0, 0);
      return ast_node(N_FOR, init, cond, body, step, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_WHILE) == 1)) {
    {
      int cond = ast_expr();
      if ((cond < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_LBRACE) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int items = 0;
      while ((input_peek() != T_RBRACE)) {
        {
          if ((input_peek() == T_EOF)) {
            return (0 - 1);
          } else {
            {
            }
          }
          int item = ast_stmt();
          if ((item < 0)) {
            return (0 - 1);
          } else {
            {
            }
          }
          if ((items == 0)) {
            items = item;
          } else {
            items = ast_link(items, item);
          }
        }
      }
      if ((input_take(T_RBRACE) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int body = ast_node(N_BLOCK, items, 0, 0, 0, 0);
      return ast_node(N_WHILE, cond, body, 0, 0, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_LBRACE) == 1)) {
    {
      int items = 0;
      while ((input_peek() != T_RBRACE)) {
        {
          if ((input_peek() == T_EOF)) {
            return (0 - 1);
          } else {
            {
            }
          }
          int item = ast_stmt();
          if ((item < 0)) {
            return (0 - 1);
          } else {
            {
            }
          }
          if ((items == 0)) {
            items = item;
          } else {
            items = ast_link(items, item);
          }
        }
      }
      if ((input_take(T_RBRACE) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return ast_node(N_BLOCK, items, 0, 0, 0, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_RETURN) == 1)) {
    {
      int value = 0;
      if ((input_peek() != T_SEMI)) {
        {
          value = ast_expr();
          if ((value < 0)) {
            return (0 - 1);
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return ast_node(N_RETURN, value, 0, 0, 0, 0);
    }
  } else {
    {
    }
  }
  int left = ast_expr();
  if ((left < 0)) {
    return (0 - 1);
  } else {
    {
    }
  }
  if ((input_take(T_EQUAL) == 1)) {
    {
      int right = ast_expr();
      if ((right < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return ast_node(N_ASSIGN, left, right, 0, 0, 0);
    }
  } else {
    {
    }
  }
  if ((input_take(T_SEMI) == 0)) {
    return (0 - 1);
  } else {
    {
    }
  }
  return ast_node(N_EXPR, left, 0, 0, 0, 0);
}

int ast_params(void) {
  int params = 0;
  if ((input_peek() == T_RPAREN)) {
    return 0;
  } else {
    {
    }
  }
  while ((1 == 1)) {
    {
      if ((input_peek() != T_ID)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int name = input_payload();
      input_pos = (input_pos + 1);
      if ((input_take(T_COLON) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int ty = ast_type();
      if ((ty == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int param = ast_node(N_PARAM, name, ty, 0, 0, (node_kind)[ty]);
      if ((params == 0)) {
        params = param;
      } else {
        params = ast_link(params, param);
      }
      if ((input_take(T_COMMA) == 0)) {
        return params;
      } else {
        {
        }
      }
    }
  }
}

int ast_struct_decl(void) {
  if ((input_peek() != T_ID)) {
    return (0 - 1);
  } else {
    {
    }
  }
  int name = input_payload();
  input_pos = (input_pos + 1);
  name = ast_decl_name(name);
  int params = 0;
  int old_scope = ast_generic_scope;
  if ((input_peek() == T_LT)) {
    {
      params = ast_generic_params();
      if ((params < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      ast_generic_scope = params;
    }
  } else {
    {
    }
  }
  if ((input_take(T_LBRACE) == 0)) {
    return (0 - 1);
  } else {
    {
    }
  }
  int fields = 0;
  while ((input_peek() != T_RBRACE)) {
    {
      if ((input_peek() == T_EOF)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_peek() != T_ID)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int field_name = input_payload();
      input_pos = (input_pos + 1);
      if ((input_take(T_COLON) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int field_type = ast_type();
      if ((field_type == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int field = ast_node(N_FIELD, field_name, field_type, 0, 0, 0);
      if ((fields == 0)) {
        fields = field;
      } else {
        fields = ast_link(fields, field);
      }
    }
  }
  if ((input_take(T_RBRACE) == 0)) {
    return (0 - 1);
  } else {
    {
    }
  }
  if ((input_take(T_SEMI) == 1)) {
    {
    }
  } else {
    {
    }
  }
  ast_generic_scope = old_scope;
  if ((params == 0)) {
    return ast_node(N_STRUCT, fields, 0, 0, name, 0);
  } else {
    {
    }
  }
  return ast_node(N_GENERIC_STRUCT, fields, 0, params, name, 0);
}

int ast_enum_decl(void) {
  if ((input_peek() != T_ID)) {
    return (0 - 1);
  } else {
    {
    }
  }
  int name = input_payload();
  input_pos = (input_pos + 1);
  name = ast_decl_name(name);
  if ((input_take(T_LBRACE) == 0)) {
    return (0 - 1);
  } else {
    {
    }
  }
  int values = 0;
  int ordinal = 0;
  while ((input_peek() != T_RBRACE)) {
    {
      if ((input_peek() == T_EOF)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_peek() != T_ID)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int member = input_payload();
      input_pos = (input_pos + 1);
      int item = ast_node(N_FIELD, member, 0, 0, ordinal, 0);
      if ((values == 0)) {
        values = item;
      } else {
        values = ast_link(values, item);
      }
      ordinal = (ordinal + 1);
      if ((input_take(T_COMMA) == 0)) {
        {
        }
      } else {
        {
        }
      }
    }
  }
  if ((input_take(T_RBRACE) == 0)) {
    return (0 - 1);
  } else {
    {
    }
  }
  if ((input_take(T_SEMI) == 1)) {
    {
    }
  } else {
    {
    }
  }
  return ast_node(N_ENUM, values, 0, 0, name, 0);
}

int ast_namespace_decl(void) {
  if ((input_peek() != T_ID)) {
    return (0 - 1);
  } else {
    {
    }
  }
  int raw = input_payload();
  input_pos = (input_pos + 1);
  int ns = raw;
  if ((ast_namespace_scope != 0)) {
    ns = sym_qualified(ast_namespace_scope, raw);
  } else {
    {
    }
  }
  if ((input_take(T_LBRACE) == 0)) {
    return (0 - 1);
  } else {
    {
    }
  }
  int old_ns = ast_namespace_scope;
  ast_namespace_scope = ns;
  int items = 0;
  while ((input_peek() != T_RBRACE)) {
    {
      if ((input_peek() == T_EOF)) {
        {
          ast_namespace_scope = old_ns;
          return (0 - 1);
        }
      } else {
        {
        }
      }
      int item = ast_decl();
      if ((item < 0)) {
        {
          ast_namespace_scope = old_ns;
          return (0 - 1);
        }
      } else {
        {
        }
      }
      if ((items == 0)) {
        items = item;
      } else {
        items = ast_link(items, item);
      }
    }
  }
  if ((input_take(T_RBRACE) == 0)) {
    {
      ast_namespace_scope = old_ns;
      return (0 - 1);
    }
  } else {
    {
    }
  }
  if ((input_take(T_SEMI) == 1)) {
    {
    }
  } else {
    {
    }
  }
  ast_namespace_scope = old_ns;
  return ast_node(N_LIST, items, 0, 0, 0, 0);
}

int ast_decl(void) {
  if ((input_take(T_NAMESPACE) == 1)) {
    return ast_namespace_decl();
  } else {
    {
    }
  }
  if ((input_take(T_EXTERN) == 1)) {
    {
      if ((input_take(T_FUNC) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_peek() != T_ID)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int name = input_payload();
      input_pos = (input_pos + 1);
      name = ast_decl_name(name);
      if ((input_take(T_LPAREN) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int params = ast_params();
      if ((params < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_RPAREN) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_COLON) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int ret_ty = ast_type();
      if ((ret_ty == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return ast_node(N_EXTERN, 0, ret_ty, params, name, (node_kind)[ret_ty]);
    }
  } else {
    {
    }
  }
  if ((input_take(T_CONST) == 1)) {
    {
      if ((input_peek() != T_ID)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int name = input_payload();
      input_pos = (input_pos + 1);
      name = ast_decl_name(name);
      if ((input_take(T_COLON) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int ty = ast_type();
      if ((ty == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_EQUAL) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int value = ast_expr();
      if ((value < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return ast_node(N_CONST, name, ty, value, 0, (node_kind)[ty]);
    }
  } else {
    {
    }
  }
  if ((input_take(T_STRUCT) == 1)) {
    return ast_struct_decl();
  } else {
    {
    }
  }
  if ((input_take(T_ENUM) == 1)) {
    return ast_enum_decl();
  } else {
    {
    }
  }
  if ((input_take(T_LET) == 1)) {
    {
      if ((input_peek() != T_ID)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int name = input_payload();
      input_pos = (input_pos + 1);
      if ((input_take(T_COLON) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int ty = ast_type();
      if ((ty == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_EQUAL) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int value = ast_expr();
      if ((value < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_SEMI) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      return ast_node(N_GLOBAL, name, ty, value, 0, (node_kind)[ty]);
    }
  } else {
    {
    }
  }
  if ((input_take(T_FUNC) == 1)) {
    {
      if ((input_peek() != T_ID)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int name = input_payload();
      input_pos = (input_pos + 1);
      name = ast_decl_name(name);
      int generic_params = 0;
      int old_scope = ast_generic_scope;
      if ((input_peek() == T_LT)) {
        {
          generic_params = ast_generic_params();
          if ((generic_params < 0)) {
            return (0 - 1);
          } else {
            {
            }
          }
          ast_generic_scope = generic_params;
        }
      } else {
        {
        }
      }
      if ((input_take(T_LPAREN) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int params = ast_params();
      if ((params < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_RPAREN) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if ((input_take(T_COLON) == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int ret_ty = ast_type();
      if ((ret_ty == 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      int body = ast_stmt();
      if ((body < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      ast_generic_scope = old_scope;
      if ((generic_params == 0)) {
        return ast_node(N_FUNC, body, ret_ty, params, name, (node_kind)[ret_ty]);
      } else {
        {
        }
      }
      return ast_node(N_GENERIC_FUNC, body, ret_ty, params, name, generic_params);
    }
  } else {
    {
    }
  }
  return (0 - 1);
}

int ast_program(void) {
  int items = 0;
  while ((input_peek() != T_EOF)) {
    {
      int item = ast_decl();
      if ((item < 0)) {
        return (0 - 1);
      } else {
        {
        }
      }
      if (((node_kind)[item] == N_LIST)) {
        {
          int nested = (node_a)[item];
          if ((nested != 0)) {
            {
              if ((items == 0)) {
                items = nested;
              } else {
                {
                  int tail = items;
                  while (((node_next)[tail] != 0)) {
                    {
                      tail = (node_next)[tail];
                    }
                  }
                  (node_next)[tail] = nested;
                }
              }
            }
          } else {
            {
            }
          }
        }
      } else {
        if ((items == 0)) {
          items = item;
        } else {
          items = ast_link(items, item);
        }
      }
    }
  }
  return ast_node(N_PROGRAM, items, 0, 0, 0, 0);
}

void c_source_reset(void) {
  c_source_len = 0;
}

void ensure_c_source(int need) {
  if ((need < c_source_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(c_source_cap, need);
  c_source = (int*)grow_ints(c_source, c_source_cap, n);
  c_source_cap = n;
}

void c_source_put(int c) {
  ensure_c_source(c_source_len);
  (c_source)[c_source_len] = c;
  c_source_len = (c_source_len + 1);
}

void source_reset(void) {
  source_len = 0;
  source_pos = 0;
}

void source_put(int c) {
  ensure_source(source_len);
  (source)[source_len] = c;
  source_len = (source_len + 1);
}

int is_space(int c) {
  if ((c == 32)) {
    return 1;
  } else {
    {
    }
  }
  if ((c == 9)) {
    return 1;
  } else {
    {
    }
  }
  if ((c == 10)) {
    return 1;
  } else {
    {
    }
  }
  if ((c == 13)) {
    return 1;
  } else {
    {
    }
  }
  return 0;
}

int is_digit(int c) {
  if ((c < 48)) {
    return 0;
  } else {
    {
    }
  }
  if ((c > 57)) {
    return 0;
  } else {
    {
    }
  }
  return 1;
}

int is_alpha(int c) {
  if ((c > 64)) {
    {
      if ((c < 91)) {
        return 1;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((c > 96)) {
    {
      if ((c < 123)) {
        return 1;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((c == 95)) {
    return 1;
  } else {
    {
    }
  }
  return 0;
}

int is_alnum(int c) {
  if ((is_alpha(c) == 1)) {
    return 1;
  } else {
    {
    }
  }
  return is_digit(c);
}

int source_peek(void) {
  if ((source_pos < source_len)) {
    return (source)[source_pos];
  } else {
    {
    }
  }
  return 0;
}

int source_take(void) {
  int c = source_peek();
  if ((source_pos < source_len)) {
    source_pos = (source_pos + 1);
  } else {
    {
    }
  }
  current_source_pos = source_pos;
  return c;
}

int span_hash(int start, int length) {
  int i = 0;
  int h = 7;
  while ((i < length)) {
    {
      h = ((h * 31) + (source)[(start + i)]);
      if ((h > 1000000)) {
        h = (h - ((h / 1000000) * 1000000));
      } else {
        {
        }
      }
      i = (i + 1);
    }
  }
  return h;
}

int span_equal(int a, int b, int length) {
  int i = 0;
  while ((i < length)) {
    {
      if (((source)[(a + i)] != (source)[(b + i)])) {
        return 0;
      } else {
        {
        }
      }
      i = (i + 1);
    }
  }
  return 1;
}

int sym_lookup(int start, int length, int h) {
  int i = 1;
  while ((i < sym_count)) {
    {
      if (((sym_len)[i] == length)) {
        {
          if (((sym_hash)[i] == h)) {
            {
              if ((span_equal((sym_start)[i], start, length) == 1)) {
                return i;
              } else {
                {
                }
              }
            }
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
      i = (i + 1);
    }
  }
  return 0;
}

int sym_qualified(int ns, int name) {
  if ((ns == 0)) {
    return name;
  } else {
    {
    }
  }
  int start = (source_len + sym_text_len);
  int out = 0;
  int i = 0;
  while ((i < (sym_len)[ns])) {
    {
      ensure_source((start + out));
      (source)[(start + out)] = (source)[((sym_start)[ns] + i)];
      out = (out + 1);
      i = (i + 1);
    }
  }
  ensure_source((start + out));
  (source)[(start + out)] = 58;
  out = (out + 1);
  ensure_source((start + out));
  (source)[(start + out)] = 58;
  out = (out + 1);
  i = 0;
  while ((i < (sym_len)[name])) {
    {
      ensure_source((start + out));
      (source)[(start + out)] = (source)[((sym_start)[name] + i)];
      out = (out + 1);
      i = (i + 1);
    }
  }
  int id = sym_intern(start, out, L_ID, 0);
  sym_text_len = (sym_text_len + out);
  return id;
}

int ast_decl_name(int name) {
  if ((ast_namespace_scope == 0)) {
    return name;
  } else {
    {
    }
  }
  return sym_qualified(ast_namespace_scope, name);
}

int ast_type_name(int name) {
  if ((ast_generic_param(name) == 1)) {
    return name;
  } else {
    {
    }
  }
  if ((ast_namespace_scope == 0)) {
    return name;
  } else {
    {
    }
  }
  return sym_qualified(ast_namespace_scope, name);
}

int sym_intern(int start, int length, int kind, int scope) {
  int h = span_hash(start, length);
  int old = sym_lookup(start, length, h);
  if ((old != 0)) {
    return old;
  } else {
    {
    }
  }
  int id = sym_count;
  ensure_sym(id);
  (sym_start)[id] = start;
  (sym_len)[id] = length;
  (sym_hash)[id] = h;
  (sym_kind)[id] = kind;
  (sym_scope)[id] = scope;
  (sym_type)[id] = 0;
  (sym_elem_kind)[id] = 0;
  (sym_elem_name)[id] = 0;
  sym_count = (sym_count + 1);
  return id;
}

int word_code(int start, int length) {
  int h = span_hash(start, length);
  if ((length == 2)) {
    {
      if ((h == 10084)) {
        return L_IF;
      } else {
        {
        }
      }
      if ((h == 9999)) {
        return L_FN;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((length == 3)) {
    {
      if ((h == 315572)) {
        return L_LET;
      } else {
        {
        }
      }
      if ((h == 312968)) {
        return L_TINT;
      } else {
        {
        }
      }
      if ((h == 310114)) {
        return L_FOR;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((length == 4)) {
    {
      if ((h == 619275)) {
        return L_FUNC;
      } else {
        {
        }
      }
      if ((h == 580992)) {
        return L_ELSE;
      } else {
        {
        }
      }
      if ((h == 582984)) {
        return L_ENUM;
      } else {
        {
        }
      }
      if ((h == 33685)) {
        return L_TRUE;
      } else {
        {
        }
      }
      if ((h == 494385)) {
        return L_TBOOL;
      } else {
        {
        }
      }
      if ((h == 90011)) {
        return L_TVOID;
      } else {
        {
        }
      }
      if ((h == 23588)) {
        return L_THEN;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((length == 5)) {
    {
      if ((h == 339014)) {
        return L_PRINT;
      } else {
        {
        }
      }
      if ((h == 505674)) {
        return L_WHILE;
      } else {
        {
        }
      }
      if ((h == 600380)) {
        return L_FALSE;
      } else {
        {
        }
      }
      if ((h == 405464)) {
        return L_BREAK;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((length == 8)) {
    {
      if (((source)[start] == 99)) {
        {
          if (((source)[(start + 1)] == 111)) {
            {
              if (((source)[(start + 2)] == 110)) {
                {
                  if (((source)[(start + 3)] == 116)) {
                    {
                      if (((source)[(start + 4)] == 105)) {
                        {
                          if (((source)[(start + 5)] == 110)) {
                            {
                              if (((source)[(start + 6)] == 117)) {
                                {
                                  if (((source)[(start + 7)] == 101)) {
                                    return L_CONTINUE;
                                  } else {
                                    {
                                    }
                                  }
                                }
                              } else {
                                {
                                }
                              }
                            }
                          } else {
                            {
                            }
                          }
                        }
                      } else {
                        {
                        }
                      }
                    }
                  } else {
                    {
                    }
                  }
                }
              } else {
                {
                }
              }
            }
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((length == 4)) {
    {
      if ((((((source)[start] == 110) && ((source)[(start + 1)] == 117)) && ((source)[(start + 2)] == 108)) && ((source)[(start + 3)] == 108))) {
        return L_NULL;
      } else {
        {
        }
      }
      if ((((((source)[start] == 99) && ((source)[(start + 1)] == 104)) && ((source)[(start + 2)] == 97)) && ((source)[(start + 3)] == 114))) {
        return L_TCHAR;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((length == 5)) {
    {
      if (((((((source)[start] == 99) && ((source)[(start + 1)] == 111)) && ((source)[(start + 2)] == 110)) && ((source)[(start + 3)] == 115)) && ((source)[(start + 4)] == 116))) {
        return L_CONST;
      } else {
        {
        }
      }
      if (((((((source)[start] == 97) && ((source)[(start + 1)] == 114)) && ((source)[(start + 2)] == 114)) && ((source)[(start + 3)] == 97)) && ((source)[(start + 4)] == 121))) {
        return L_ARRAY;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((length == 6)) {
    {
      if ((h == 448999)) {
        return L_EXTERN;
      } else {
        {
        }
      }
      if (((source)[start] == 114)) {
        {
          if (((source)[(start + 1)] == 101)) {
            {
              if (((source)[(start + 2)] == 116)) {
                {
                  if (((source)[(start + 3)] == 117)) {
                    {
                      if (((source)[(start + 4)] == 114)) {
                        {
                          if (((source)[(start + 5)] == 110)) {
                            return L_RETURN;
                          } else {
                            {
                            }
                          }
                        }
                      } else {
                        {
                        }
                      }
                    }
                  } else {
                    {
                    }
                  }
                }
              } else {
                {
                }
              }
            }
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
      if (((source)[start] == 115)) {
        {
          if (((source)[(start + 1)] == 116)) {
            {
              if (((source)[(start + 2)] == 114)) {
                {
                  if (((source)[(start + 3)] == 117)) {
                    {
                      if (((source)[(start + 4)] == 99)) {
                        {
                          if (((source)[(start + 5)] == 116)) {
                            return L_STRUCT;
                          } else {
                            {
                            }
                          }
                        }
                      } else {
                        {
                        }
                      }
                    }
                  } else {
                    {
                    }
                  }
                }
              } else {
                {
                }
              }
            }
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
      if (((source)[start] == 115)) {
        {
          if (((source)[(start + 1)] == 116)) {
            {
              if (((source)[(start + 2)] == 114)) {
                {
                  if (((source)[(start + 3)] == 105)) {
                    {
                      if (((source)[(start + 4)] == 110)) {
                        {
                          if (((source)[(start + 5)] == 103)) {
                            return L_TSTRING;
                          } else {
                            {
                            }
                          }
                        }
                      } else {
                        {
                        }
                      }
                    }
                  } else {
                    {
                    }
                  }
                }
              } else {
                {
                }
              }
            }
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((length == 5)) {
    {
      if (((((((source)[start] == 102) && ((source)[(start + 1)] == 108)) && ((source)[(start + 2)] == 111)) && ((source)[(start + 3)] == 97)) && ((source)[(start + 4)] == 116))) {
        return L_TFLOAT;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((length == 9)) {
    {
      if (((((((((((source)[start] == 110) && ((source)[(start + 1)] == 97)) && ((source)[(start + 2)] == 109)) && ((source)[(start + 3)] == 101)) && ((source)[(start + 4)] == 115)) && ((source)[(start + 5)] == 112)) && ((source)[(start + 6)] == 97)) && ((source)[(start + 7)] == 99)) && ((source)[(start + 8)] == 101))) {
        return L_NAMESPACE;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((length == 6)) {
    {
      if ((((((((source)[start] == 100) && ((source)[(start + 1)] == 111)) && ((source)[(start + 2)] == 117)) && ((source)[(start + 3)] == 98)) && ((source)[(start + 4)] == 108)) && ((source)[(start + 5)] == 101))) {
        return L_TDOUBLE;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  return L_ID;
}

void lexer_skip(void) {
  while ((is_space(source_peek()) == 1)) {
    {
      source_take();
    }
  }
  if ((source_peek() == 47)) {
    {
      if (((source_pos + 1) < source_len)) {
        {
          if (((source)[(source_pos + 1)] == 47)) {
            {
              while ((source_peek() != 10)) {
                {
                  if ((source_pos > (source_len - 1))) {
                    return;
                  } else {
                    {
                    }
                  }
                  source_take();
                }
              }
              lexer_skip();
            }
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
}

int lexer_next(void) {
  lexer_skip();
  tok_start = source_pos;
  tok_length = 0;
  int c = source_peek();
  if ((c == 0)) {
    {
      tok_kind = L_EOF;
      tok_value = 0;
      return tok_kind;
    }
  } else {
    {
    }
  }
  if ((is_alpha(c) == 1)) {
    {
      while ((is_alnum(source_peek()) == 1)) {
        {
          source_take();
        }
      }
      tok_length = (source_pos - tok_start);
      tok_kind = word_code(tok_start, tok_length);
      if ((tok_kind == L_ID)) {
        tok_value = sym_intern(tok_start, tok_length, L_ID, 0);
      } else {
        tok_value = 0;
      }
      return tok_kind;
    }
  } else {
    {
    }
  }
  if ((c == 39)) {
    {
      source_take();
      int v = source_take();
      if ((v == 92)) {
        {
          int e = source_take();
          if ((e == 110)) {
            v = 10;
          } else {
            if ((e == 116)) {
              v = 9;
            } else {
              if ((e == 114)) {
                v = 13;
              } else {
                if ((e == 98)) {
                  v = 8;
                } else {
                  if ((e == 102)) {
                    v = 12;
                  } else {
                    if ((e == 118)) {
                      v = 11;
                    } else {
                      if ((e == 48)) {
                        v = 0;
                      } else {
                        v = e;
                      }
                    }
                  }
                }
              }
            }
          }
        }
      } else {
        {
        }
      }
      if ((source_peek() != 39)) {
        {
          tok_kind = L_EOF;
          tok_value = 0;
          return tok_kind;
        }
      } else {
        {
        }
      }
      source_take();
      tok_kind = L_CHAR;
      tok_value = v;
      tok_length = 3;
      return tok_kind;
    }
  } else {
    {
    }
  }
  if ((c == 34)) {
    {
      source_take();
      while ((source_peek() != 34)) {
        {
          if ((source_pos > (source_len - 1))) {
            {
              tok_kind = L_EOF;
              tok_value = 0;
              return tok_kind;
            }
          } else {
            {
            }
          }
          if ((source_peek() == 92)) {
            {
              source_take();
              if ((source_peek() != 0)) {
                source_take();
              } else {
                {
                }
              }
            }
          } else {
            source_take();
          }
        }
      }
      source_take();
      tok_kind = L_STRING;
      tok_length = (source_pos - tok_start);
      tok_value = sym_intern((tok_start + 1), (tok_length - 2), L_STRING, 0);
      return tok_kind;
    }
  } else {
    {
    }
  }
  if ((is_digit(c) == 1)) {
    {
      int value = 0;
      while ((is_digit(source_peek()) == 1)) {
        {
          value = (((value * 10) + source_take()) - 48);
        }
      }
      if ((source_peek() == 46)) {
        {
          source_take();
          while ((is_digit(source_peek()) == 1)) {
            {
              source_take();
            }
          }
          tok_kind = L_FLOAT;
          tok_length = (source_pos - tok_start);
          tok_value = sym_intern(tok_start, tok_length, L_FLOAT, 0);
          return tok_kind;
        }
      } else {
        {
        }
      }
      tok_kind = L_INT;
      tok_value = value;
      tok_length = (source_pos - tok_start);
      return tok_kind;
    }
  } else {
    {
    }
  }
  source_take();
  tok_length = 1;
  if ((c == 43)) {
    {
      if ((source_peek() == 43)) {
        {
          source_take();
          tok_kind = L_CONCAT;
        }
      } else {
        tok_kind = L_PLUS;
      }
    }
  } else {
    if ((c == 45)) {
      tok_kind = L_MINUS;
    } else {
      if ((c == 42)) {
        tok_kind = L_STAR;
      } else {
        if ((c == 47)) {
          tok_kind = L_DIV;
        } else {
          if ((c == 37)) {
            tok_kind = L_MOD;
          } else {
            if ((c == 40)) {
              tok_kind = L_LPAREN;
            } else {
              if ((c == 41)) {
                tok_kind = L_RPAREN;
              } else {
                if ((c == 123)) {
                  tok_kind = L_LBRACE;
                } else {
                  if ((c == 125)) {
                    tok_kind = L_RBRACE;
                  } else {
                    if ((c == 58)) {
                      {
                        if ((source_peek() == 58)) {
                          {
                            source_take();
                            tok_kind = L_SCOPE;
                          }
                        } else {
                          tok_kind = L_COLON;
                        }
                      }
                    } else {
                      if ((c == 59)) {
                        tok_kind = L_SEMI;
                      } else {
                        if ((c == 44)) {
                          tok_kind = L_COMMA;
                        } else {
                          if ((c == 91)) {
                            tok_kind = L_LBRACK;
                          } else {
                            if ((c == 93)) {
                              tok_kind = L_RBRACK;
                            } else {
                              if ((c == 46)) {
                                tok_kind = L_DOT;
                              } else {
                                if ((c == 60)) {
                                  {
                                    if ((source_peek() == 60)) {
                                      {
                                        source_take();
                                        tok_kind = L_SHL;
                                      }
                                    } else {
                                      tok_kind = L_LT;
                                    }
                                  }
                                } else {
                                  if ((c == 62)) {
                                    {
                                      if ((source_peek() == 62)) {
                                        {
                                          source_take();
                                          tok_kind = L_SHR;
                                        }
                                      } else {
                                        tok_kind = L_GT;
                                      }
                                    }
                                  } else {
                                    if ((c == 124)) {
                                      {
                                        if ((source_peek() == 124)) {
                                          {
                                            source_take();
                                            tok_kind = L_OR;
                                          }
                                        } else {
                                          tok_kind = L_BITOR;
                                        }
                                      }
                                    } else {
                                      if ((c == 94)) {
                                        tok_kind = L_BITXOR;
                                      } else {
                                        if ((c == 126)) {
                                          tok_kind = L_BITNOT;
                                        } else {
                                          if ((c == 61)) {
                                            {
                                              if ((source_peek() == 61)) {
                                                {
                                                  source_take();
                                                  tok_kind = L_EQEQ;
                                                }
                                              } else {
                                                tok_kind = L_EQ;
                                              }
                                            }
                                          } else {
                                            if ((c == 33)) {
                                              {
                                                if ((source_peek() == 61)) {
                                                  {
                                                    source_take();
                                                    tok_kind = L_NEQ;
                                                  }
                                                } else {
                                                  tok_kind = L_NEQ;
                                                }
                                              }
                                            } else {
                                              if ((c == 38)) {
                                                {
                                                  if ((source_peek() == 38)) {
                                                    {
                                                      source_take();
                                                      tok_kind = L_AND;
                                                    }
                                                  } else {
                                                    tok_kind = L_AMP;
                                                  }
                                                }
                                              } else {
                                                if ((c == 124)) {
                                                  {
                                                    if ((source_peek() == 124)) {
                                                      {
                                                        source_take();
                                                        tok_kind = L_OR;
                                                      }
                                                    } else {
                                                      tok_kind = L_EOF;
                                                    }
                                                  }
                                                } else {
                                                  tok_kind = L_EOF;
                                                }
                                              }
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  tok_value = 0;
  return tok_kind;
}

void include_process_line(int* line, int length) {
  int mode = 0;
  int p = 0;
  while (((p < length) && (((line)[p] == 32) || ((line)[p] == 9)))) {
    {
      p = (p + 1);
    }
  }
  if (((p + 7) < length)) {
    {
      if (((((((((line)[p] == 105) && ((line)[(p + 1)] == 110)) && ((line)[(p + 2)] == 99)) && ((line)[(p + 3)] == 108)) && ((line)[(p + 4)] == 117)) && ((line)[(p + 5)] == 100)) && ((line)[(p + 6)] == 101))) {
        {
          if ((((line)[(p + 7)] == 32) || ((line)[(p + 7)] == 9))) {
            mode = 1;
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((((p + 8) < length) && (mode == 0))) {
    {
      if ((((((((((line)[p] == 105) && ((line)[(p + 1)] == 110)) && ((line)[(p + 2)] == 99)) && ((line)[(p + 3)] == 108)) && ((line)[(p + 4)] == 117)) && ((line)[(p + 5)] == 100)) && ((line)[(p + 6)] == 101)) && ((line)[(p + 7)] == 99))) {
        {
          if ((((line)[(p + 8)] == 32) || ((line)[(p + 8)] == 9))) {
            mode = 2;
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((mode == 0)) {
    {
      int i = 0;
      while ((i < length)) {
        {
          source_put((line)[i]);
          i = (i + 1);
        }
      }
      source_put(10);
    }
  } else {
    {
      int* child = ash_include_open_line(line, length, mode);
      if ((child == 0)) {
        {
          if ((ash_include_last_status() == 1)) {
            include_ok = 0;
          } else {
            {
            }
          }
        }
      } else {
        {
          if ((mode == 1)) {
            {
              include_expand_handle(child);
            }
          } else {
            {
              int c = read_char(child);
              while ((c != (0 - 1))) {
                {
                  c_source_put(c);
                  c = read_char(child);
                }
              }
            }
          }
          (int)close_file(child);
          ash_include_close();
        }
      }
    }
  }
}

void include_expand_handle(int* handle) {
  int* line = (int*)alloc_ints(include_line_cap);
  int length = 0;
  int c = read_char(handle);
  while ((c != (0 - 1))) {
    {
      if ((c == 10)) {
        {
          include_process_line(line, length);
          length = 0;
        }
      } else {
        {
          if ((length < include_line_cap)) {
            {
              (line)[length] = c;
              length = (length + 1);
            }
          } else {
            include_ok = 0;
          }
        }
      }
      c = read_char(handle);
    }
  }
  if ((length > 0)) {
    include_process_line(line, length);
  } else {
    {
    }
  }
}

void load_source_file(char* path) {
  source_reset();
  int* handle = (void*)open_file(path, "r");
  int c = read_char(handle);
  while ((c != (0 - 1))) {
    {
      source_put(c);
      c = read_char(handle);
    }
  }
  (int)close_file(handle);
}

int map_token(int k) {
  if ((k == L_EOF)) {
    return T_EOF;
  } else {
    {
    }
  }
  if ((k == L_ID)) {
    return T_ID;
  } else {
    {
    }
  }
  if ((k == L_INT)) {
    return T_INT;
  } else {
    {
    }
  }
  if ((k == L_STRING)) {
    return T_STRING;
  } else {
    {
    }
  }
  if ((k == L_FUNC)) {
    return T_FUNC;
  } else {
    {
    }
  }
  if ((k == L_EXTERN)) {
    return T_EXTERN;
  } else {
    {
    }
  }
  if ((k == L_LET)) {
    return T_LET;
  } else {
    {
    }
  }
  if ((k == L_PRINT)) {
    return T_PRINT;
  } else {
    {
    }
  }
  if ((k == L_RETURN)) {
    return T_RETURN;
  } else {
    {
    }
  }
  if ((k == L_IF)) {
    return T_IF;
  } else {
    {
    }
  }
  if ((k == L_ELSE)) {
    return T_ELSE;
  } else {
    {
    }
  }
  if ((k == L_WHILE)) {
    return T_WHILE;
  } else {
    {
    }
  }
  if ((k == L_FOR)) {
    return T_FOR;
  } else {
    {
    }
  }
  if ((k == L_STRUCT)) {
    return T_STRUCT;
  } else {
    {
    }
  }
  if ((k == L_ENUM)) {
    return T_ENUM;
  } else {
    {
    }
  }
  if ((k == L_BREAK)) {
    return T_BREAK;
  } else {
    {
    }
  }
  if ((k == L_CONTINUE)) {
    return T_CONTINUE;
  } else {
    {
    }
  }
  if ((k == L_TRUE)) {
    return T_TRUE;
  } else {
    {
    }
  }
  if ((k == L_FALSE)) {
    return T_FALSE;
  } else {
    {
    }
  }
  if ((k == L_TINT)) {
    return T_TINT;
  } else {
    {
    }
  }
  if ((k == L_TBOOL)) {
    return T_TBOOL;
  } else {
    {
    }
  }
  if ((k == L_TSTRING)) {
    return T_TSTRING;
  } else {
    {
    }
  }
  if ((k == L_TVOID)) {
    return T_TVOID;
  } else {
    {
    }
  }
  if ((k == L_THEN)) {
    return T_THEN;
  } else {
    {
    }
  }
  if ((k == L_PLUS)) {
    return T_PLUS;
  } else {
    {
    }
  }
  if ((k == L_MINUS)) {
    return T_MINUS;
  } else {
    {
    }
  }
  if ((k == L_STAR)) {
    return T_STAR;
  } else {
    {
    }
  }
  if ((k == L_DIV)) {
    return T_DIVIDE;
  } else {
    {
    }
  }
  if ((k == L_MOD)) {
    return T_MOD;
  } else {
    {
    }
  }
  if ((k == L_CONCAT)) {
    return T_CONCAT;
  } else {
    {
    }
  }
  if ((k == L_AND)) {
    return T_AND_AND;
  } else {
    {
    }
  }
  if ((k == L_OR)) {
    return T_OR_OR;
  } else {
    {
    }
  }
  if ((k == L_EQ)) {
    return T_EQUAL;
  } else {
    {
    }
  }
  if ((k == L_EQEQ)) {
    return T_EQEQ;
  } else {
    {
    }
  }
  if ((k == L_NEQ)) {
    return T_NEQ;
  } else {
    {
    }
  }
  if ((k == L_LT)) {
    return T_LT;
  } else {
    {
    }
  }
  if ((k == L_GT)) {
    return T_GT;
  } else {
    {
    }
  }
  if ((k == L_COLON)) {
    return T_COLON;
  } else {
    {
    }
  }
  if ((k == L_LPAREN)) {
    return T_LPAREN;
  } else {
    {
    }
  }
  if ((k == L_RPAREN)) {
    return T_RPAREN;
  } else {
    {
    }
  }
  if ((k == L_LBRACE)) {
    return T_LBRACE;
  } else {
    {
    }
  }
  if ((k == L_RBRACE)) {
    return T_RBRACE;
  } else {
    {
    }
  }
  if ((k == L_SEMI)) {
    return T_SEMI;
  } else {
    {
    }
  }
  if ((k == L_COMMA)) {
    return T_COMMA;
  } else {
    {
    }
  }
  if ((k == L_AMP)) {
    return T_AMP;
  } else {
    {
    }
  }
  if ((k == L_LBRACK)) {
    return T_LBRACK;
  } else {
    {
    }
  }
  if ((k == L_RBRACK)) {
    return T_RBRACK;
  } else {
    {
    }
  }
  if ((k == L_DOT)) {
    return T_DOT;
  } else {
    {
    }
  }
  if ((k == L_CHAR)) {
    return T_CHAR;
  } else {
    {
    }
  }
  if ((k == L_NULL)) {
    return T_NULL;
  } else {
    {
    }
  }
  if ((k == L_CONST)) {
    return T_CONST;
  } else {
    {
    }
  }
  if ((k == L_TCHAR)) {
    return T_TCHAR;
  } else {
    {
    }
  }
  if ((k == L_FLOAT)) {
    return T_FLOAT;
  } else {
    {
    }
  }
  if ((k == L_TFLOAT)) {
    return T_FLOAT;
  } else {
    {
    }
  }
  if ((k == L_TDOUBLE)) {
    return T_TDOUBLE;
  } else {
    {
    }
  }
  if ((k == L_BITOR)) {
    return T_BITOR;
  } else {
    {
    }
  }
  if ((k == L_BITXOR)) {
    return T_BITXOR;
  } else {
    {
    }
  }
  if ((k == L_BITNOT)) {
    return T_BITNOT;
  } else {
    {
    }
  }
  if ((k == L_SHL)) {
    return T_SHL;
  } else {
    {
    }
  }
  if ((k == L_SHR)) {
    return T_SHR;
  } else {
    {
    }
  }
  if ((k == L_FN)) {
    return T_FN;
  } else {
    {
    }
  }
  if ((k == L_ARRAY)) {
    return T_ARRAY;
  } else {
    {
    }
  }
  if ((k == L_NAMESPACE)) {
    return T_NAMESPACE;
  } else {
    {
    }
  }
  if ((k == L_SCOPE)) {
    return T_SCOPE;
  } else {
    {
    }
  }
  return T_EOF;
}

void load_tokens_from_file(char* path) {
  input_reset();
  ash_include_reset_session();
  source_reset();
  sym_text_len = 0;
  sym_count = 1;
  ast_namespace_scope = 0;
  c_source_reset();
  include_ok = 1;
  int* handle = ash_include_open_root(path);
  if ((handle == 0)) {
    include_ok = 0;
  } else {
    {
      include_expand_handle(handle);
      (int)close_file(handle);
      ash_include_close();
    }
  }
  int k = lexer_next();
  while ((k != L_EOF)) {
    {
      if ((debug_tokens == 1)) {
        {
          printf("%d\n", map_token(k));
          printf("%d\n", tok_start);
          printf("%d\n", tok_length);
          printf("%d\n", tok_value);
        }
      } else {
        {
        }
      }
      input_put(map_token(k), tok_value);
      k = lexer_next();
    }
  }
  input_put(T_EOF, 0);
}

void ensure_tc_vars(int need) {
  if ((need < tc_var_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(tc_var_cap, need);
  tc_var_name = (int*)grow_ints(tc_var_name, tc_var_cap, n);
  tc_var_kind = (int*)grow_ints(tc_var_kind, tc_var_cap, n);
  tc_var_named = (int*)grow_ints(tc_var_named, tc_var_cap, n);
  tc_var_elem_kind = (int*)grow_ints(tc_var_elem_kind, tc_var_cap, n);
  tc_var_elem_name = (int*)grow_ints(tc_var_elem_name, tc_var_cap, n);
  tc_var_type = (int*)grow_ints(tc_var_type, tc_var_cap, n);
  tc_var_owned = (int*)grow_ints(tc_var_owned, tc_var_cap, n);
  tc_var_moved = (int*)grow_ints(tc_var_moved, tc_var_cap, n);
  tc_var_borrow_count = (int*)grow_ints(tc_var_borrow_count, tc_var_cap, n);
  tc_var_borrow_source = (int*)grow_ints(tc_var_borrow_source, tc_var_cap, n);
  tc_var_cap = n;
}

void ensure_tc_scopes(int need) {
  if ((need < tc_scope_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(tc_scope_cap, need);
  tc_scope_start = (int*)grow_ints(tc_scope_start, tc_scope_cap, n);
  tc_scope_cap = n;
}

void tc_enter_scope(void) {
  ensure_tc_scopes(tc_scope_count);
  (tc_scope_start)[tc_scope_count] = tc_var_count;
  tc_scope_count = (tc_scope_count + 1);
}

void tc_leave_scope(void) {
  int begin = 0;
  int i = 0;
  int source_index = 0;
  if ((tc_scope_count == 0)) {
    return;
  } else {
    {
    }
  }
  tc_scope_count = (tc_scope_count - 1);
  begin = (tc_scope_start)[tc_scope_count];
  i = begin;
  while ((i < tc_var_count)) {
    {
      source_index = (tc_var_borrow_source)[i];
      if ((source_index < 0)) {
        {
        }
      } else {
        if (((source_index < begin) && ((tc_var_borrow_count)[source_index] > 0))) {
          (tc_var_borrow_count)[source_index] = ((tc_var_borrow_count)[source_index] - 1);
        } else {
          {
          }
        }
      }
      i = (i + 1);
    }
  }
  tc_var_count = begin;
}

void ensure_tc_path(int need) {
  if ((need < tc_path_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(tc_path_cap, need);
  tc_path_name = (int*)grow_ints(tc_path_name, tc_path_cap, n);
  tc_path_cap = n;
}

void tc_fail(int code) {
  if ((tc_ok == 1)) {
    {
      tc_ok = 0;
      tc_error_code = code;
      tc_error_pos = current_source_pos;
    }
  } else {
    {
    }
  }
}

void ensure_tc_bindings(int need) {
  if ((need < tc_bind_cap)) {
    return;
  } else {
    {
    }
  }
  int n = next_capacity(tc_bind_cap, need);
  tc_bind_name = (int*)grow_ints(tc_bind_name, tc_bind_cap, n);
  tc_bind_type = (int*)grow_ints(tc_bind_type, tc_bind_cap, n);
  tc_bind_cap = n;
}

void tc_bind_clear(void) {
  tc_bind_count = 0;
}

int tc_bind_find(int name) {
  int i = 0;
  while ((i < tc_bind_count)) {
    {
      if (((tc_bind_name)[i] == name)) {
        return (tc_bind_type)[i];
      } else {
        {
        }
      }
      i = (i + 1);
    }
  }
  return 0;
}

int tc_bind_add(int name, int ty) {
  int old = tc_bind_find(name);
  if ((old != 0)) {
    {
      if ((tc_type_equal(old, ty) == 0)) {
        {
          tc_fail(12);
          return 0;
        }
      } else {
        {
        }
      }
      return 1;
    }
  } else {
    {
    }
  }
  ensure_tc_bindings(tc_bind_count);
  (tc_bind_name)[tc_bind_count] = name;
  (tc_bind_type)[tc_bind_count] = ty;
  tc_bind_count = (tc_bind_count + 1);
  return 1;
}

int tc_type_equal(int a, int b) {
  if (((a == 0) || (b == 0))) {
    return 0;
  } else {
    {
    }
  }
  int ak = (node_kind)[a];
  int bk = (node_kind)[b];
  if (((ak == TY_PARAM) || (bk == TY_PARAM))) {
    {
      if (((ak == bk) && ((node_value)[a] == (node_value)[b]))) {
        return 1;
      } else {
        {
        }
      }
      return 0;
    }
  } else {
    {
    }
  }
  if ((ak != bk)) {
    return 0;
  } else {
    {
    }
  }
  if ((((((((ak == TY_INT) || (ak == TY_BOOL)) || (ak == TY_STRING)) || (ak == TY_CHAR)) || (ak == TY_FLOAT)) || (ak == TY_DOUBLE)) || (ak == TY_VOID))) {
    return 1;
  } else {
    {
    }
  }
  if ((ak == TY_NAMED)) {
    {
      if (((node_value)[a] == (node_value)[b])) {
        return 1;
      } else {
        {
        }
      }
      return 0;
    }
  } else {
    {
    }
  }
  if ((ak == TY_PTR)) {
    return tc_type_equal((node_a)[a], (node_a)[b]);
  } else {
    {
    }
  }
  if ((ak == TY_ARRAY)) {
    return (((node_value)[a] == (node_value)[b]) && tc_type_equal((node_a)[a], (node_a)[b]));
  } else {
    {
    }
  }
  if ((ak == TY_DYN_ARRAY)) {
    return tc_type_equal((node_a)[a], (node_a)[b]);
  } else {
    {
    }
  }
  if ((ak == TY_GENERIC)) {
    {
      if (((node_value)[a] != (node_value)[b])) {
        return 0;
      } else {
        {
        }
      }
      int x = (node_a)[a];
      int y = (node_a)[b];
      while (((x != 0) && (y != 0))) {
        {
          if ((tc_type_equal(x, y) == 0)) {
            return 0;
          } else {
            {
            }
          }
          x = (node_next)[x];
          y = (node_next)[y];
        }
      }
      if (((x != 0) || (y != 0))) {
        return 0;
      } else {
        {
        }
      }
      return 1;
    }
  } else {
    {
    }
  }
  return 1;
}

int tc_type_node_from_summary(int kind, int name, int elem_kind, int elem_name) {
  if (((kind == TY_GENERIC) && (name != 0))) {
    return name;
  } else {
    {
    }
  }
  if ((kind == TY_NAMED)) {
    return ast_node(TY_NAMED, 0, 0, 0, name, 0);
  } else {
    {
    }
  }
  if ((kind == TY_PTR)) {
    return ast_node(TY_PTR, tc_type_node_from_summary(elem_kind, elem_name, 0, 0), 0, 0, 0, 0);
  } else {
    {
    }
  }
  if ((kind == TY_DYN_ARRAY)) {
    return ast_node(TY_DYN_ARRAY, tc_type_node_from_summary(elem_kind, elem_name, 0, 0), 0, 0, 0, 0);
  } else {
    {
    }
  }
  return ast_node(kind, 0, 0, 0, 0, 0);
}

void tc_match_generic(int formal, int actual) {
  if (((formal == 0) || (actual == 0))) {
    {
      tc_fail(12);
      return;
    }
  } else {
    {
    }
  }
  if (((node_kind)[formal] == TY_PARAM)) {
    {
      if ((tc_bind_add((node_value)[formal], actual) == 0)) {
        return;
      } else {
        {
        }
      }
      return;
    }
  } else {
    {
    }
  }
  if (((node_kind)[formal] == TY_GENERIC)) {
    {
      if ((((node_kind)[actual] != TY_GENERIC) || ((node_value)[formal] != (node_value)[actual]))) {
        {
          tc_fail(12);
          return;
        }
      } else {
        {
        }
      }
      int f = (node_a)[formal];
      int a = (node_a)[actual];
      while (((f != 0) && (a != 0))) {
        {
          tc_match_generic(f, a);
          f = (node_next)[f];
          a = (node_next)[a];
        }
      }
      if (((f != 0) || (a != 0))) {
        tc_fail(13);
      } else {
        {
        }
      }
      return;
    }
  } else {
    {
    }
  }
  tc_type_node(formal);
  if ((tc_type_equal(formal, actual) == 0)) {
    tc_fail(12);
  } else {
    {
    }
  }
}

int tc_substitute_type(int ty) {
  if ((ty == 0)) {
    return 0;
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_PARAM)) {
    {
      int b = tc_bind_find((node_value)[ty]);
      if ((b != 0)) {
        return tc_substitute_type(b);
      } else {
        {
        }
      }
      return ast_node(TY_PARAM, (node_a)[ty], (node_b)[ty], (node_c)[ty], (node_value)[ty], (node_aux)[ty]);
    }
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_PTR)) {
    return ast_node(TY_PTR, tc_substitute_type((node_a)[ty]), 0, 0, 0, 0);
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_ARRAY)) {
    return ast_node(TY_ARRAY, tc_substitute_type((node_a)[ty]), 0, 0, (node_value)[ty], 0);
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_DYN_ARRAY)) {
    return ast_node(TY_DYN_ARRAY, tc_substitute_type((node_a)[ty]), 0, 0, 0, 0);
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_GENERIC)) {
    {
      int args = 0;
      int p = (node_a)[ty];
      while ((p != 0)) {
        {
          int q = tc_substitute_type(p);
          if ((args == 0)) {
            args = q;
          } else {
            args = ast_link(args, q);
          }
          p = (node_next)[p];
        }
      }
      return ast_node(TY_GENERIC, args, 0, 0, (node_value)[ty], 0);
    }
  } else {
    {
    }
  }
  return ast_node((node_kind)[ty], (node_a)[ty], (node_b)[ty], (node_c)[ty], (node_value)[ty], (node_aux)[ty]);
}

int tc_same(int a_kind, int a_name, int b_kind, int b_name) {
  if (((a_kind == TY_PTR) && (b_kind == TY_PTR))) {
    return 1;
  } else {
    {
    }
  }
  if ((a_kind == b_kind)) {
    {
      if ((a_kind == TY_NAMED)) {
        {
          if ((a_name == b_name)) {
            return 1;
          } else {
            {
            }
          }
          return 0;
        }
      } else {
        {
        }
      }
      return 1;
    }
  } else {
    {
    }
  }
  if ((((a_kind == TY_FLOAT) || (a_kind == TY_DOUBLE)) && ((b_kind == TY_FLOAT) || (b_kind == TY_DOUBLE)))) {
    return 1;
  } else {
    {
    }
  }
  if ((((a_kind == TY_INT) || (a_kind == TY_BOOL)) && ((b_kind == TY_INT) || (b_kind == TY_BOOL)))) {
    return 1;
  } else {
    {
    }
  }
  if (((a_kind == TY_VOID) && (b_kind == TY_INT))) {
    return 1;
  } else {
    {
    }
  }
  if (((a_kind == TY_INT) && (b_kind == TY_VOID))) {
    return 1;
  } else {
    {
    }
  }
  return 0;
}

int tc_array_elem_same(int a_kind, int a_name, int b_kind, int b_name) {
  if ((((a_kind == TY_FLOAT) || (a_kind == TY_DOUBLE)) && ((b_kind == TY_FLOAT) || (b_kind == TY_DOUBLE)))) {
    return 1;
  } else {
    {
    }
  }
  if ((a_kind != b_kind)) {
    return 0;
  } else {
    {
    }
  }
  if ((a_kind == TY_NAMED)) {
    {
      if ((a_name == b_name)) {
        return 1;
      } else {
        {
        }
      }
      return 0;
    }
  } else {
    {
    }
  }
  return 1;
}

int tc_same_full(int a_kind, int a_name, int a_elem_kind, int a_elem_name, int b_kind, int b_name, int b_elem_kind, int b_elem_name) {
  if (((a_kind == TY_PARAM) || (b_kind == TY_PARAM))) {
    {
      if (((a_kind == TY_PARAM) && (b_kind == TY_PARAM))) {
        return 1;
      } else {
        {
        }
      }
      return 1;
    }
  } else {
    {
    }
  }
  if (((a_kind == TY_GENERIC) || (b_kind == TY_GENERIC))) {
    {
      if (((a_kind == TY_GENERIC) && (b_kind == TY_GENERIC))) {
        return tc_type_equal(a_name, b_name);
      } else {
        {
        }
      }
      return 0;
    }
  } else {
    {
    }
  }
  if (((a_kind == TY_PTR) && (b_kind == TY_PTR))) {
    {
      if (((a_elem_kind == TY_VOID) || (b_elem_kind == TY_VOID))) {
        return 1;
      } else {
        {
        }
      }
      if ((a_elem_kind != b_elem_kind)) {
        return 0;
      } else {
        {
        }
      }
      if (((a_elem_kind == TY_NAMED) && (a_elem_name != b_elem_name))) {
        return 0;
      } else {
        {
        }
      }
      return 1;
    }
  } else {
    {
    }
  }
  if ((a_kind == b_kind)) {
    {
      if ((a_kind == TY_NAMED)) {
        {
          if ((a_name == b_name)) {
            return 1;
          } else {
            {
            }
          }
          return 0;
        }
      } else {
        {
        }
      }
      if ((a_kind == TY_DYN_ARRAY)) {
        {
          if ((a_elem_kind != b_elem_kind)) {
            return 0;
          } else {
            {
            }
          }
          if (((a_elem_kind == TY_NAMED) && (a_elem_name != b_elem_name))) {
            return 0;
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
      return 1;
    }
  } else {
    {
    }
  }
  if ((((a_kind == TY_FLOAT) || (a_kind == TY_DOUBLE)) && ((b_kind == TY_FLOAT) || (b_kind == TY_DOUBLE)))) {
    return 1;
  } else {
    {
    }
  }
  if ((((a_kind == TY_INT) || (a_kind == TY_BOOL)) && ((b_kind == TY_INT) || (b_kind == TY_BOOL)))) {
    return 1;
  } else {
    {
    }
  }
  if ((((a_kind == TY_CHAR) && ((b_kind == TY_INT) || (b_kind == TY_BOOL))) || ((b_kind == TY_CHAR) && ((a_kind == TY_INT) || (a_kind == TY_BOOL))))) {
    return 1;
  } else {
    {
    }
  }
  if ((((a_kind == TY_VOID) && (b_kind == TY_INT)) || ((a_kind == TY_INT) && (b_kind == TY_VOID)))) {
    return 1;
  } else {
    {
    }
  }
  return 0;
}

int tc_ptr_diff_ok(int a_kind, int a_elem_kind, int a_elem_name, int b_kind, int b_elem_kind, int b_elem_name) {
  if (((a_kind != TY_PTR) || (b_kind != TY_PTR))) {
    return 0;
  } else {
    {
    }
  }
  if (((a_elem_kind == TY_VOID) || (b_elem_kind == TY_VOID))) {
    return 0;
  } else {
    {
    }
  }
  if ((a_elem_kind != b_elem_kind)) {
    return 0;
  } else {
    {
    }
  }
  if (((a_elem_kind == TY_NAMED) && (a_elem_name != b_elem_name))) {
    return 0;
  } else {
    {
    }
  }
  return 1;
}

int sym_suffix_equal(int full, int base) {
  if (((sym_len)[full] == (sym_len)[base])) {
    {
      int i = 0;
      while ((i < (sym_len)[base])) {
        {
          if (((source)[((sym_start)[full] + i)] != (source)[((sym_start)[base] + i)])) {
            return 0;
          } else {
            {
            }
          }
          i = (i + 1);
        }
      }
      return 1;
    }
  } else {
    {
    }
  }
  if (((sym_len)[full] < ((sym_len)[base] + 3))) {
    return 0;
  } else {
    {
    }
  }
  int start = (((sym_start)[full] + (sym_len)[full]) - (sym_len)[base]);
  if ((((source)[(start - 1)] != 58) || ((source)[(start - 2)] != 58))) {
    return 0;
  } else {
    {
    }
  }
  int j = 0;
  while ((j < (sym_len)[base])) {
    {
      if (((source)[(start + j)] != (source)[((sym_start)[base] + j)])) {
        return 0;
      } else {
        {
        }
      }
      j = (j + 1);
    }
  }
  return 1;
}

int tc_find_struct(int name) {
  int item = (node_a)[tc_root];
  while ((item != 0)) {
    {
      if (((((node_kind)[item] == N_STRUCT) || ((node_kind)[item] == N_GENERIC_STRUCT)) && ((node_value)[item] == name))) {
        return item;
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  return 0;
}

int tc_find_struct_ctx(int name, int ns) {
  int exact = tc_find_struct(name);
  if ((exact != 0)) {
    return exact;
  } else {
    {
    }
  }
  if ((ns == 0)) {
    return 0;
  } else {
    {
    }
  }
  int item = (node_a)[tc_root];
  while ((item != 0)) {
    {
      if ((((((node_kind)[item] == N_STRUCT) || ((node_kind)[item] == N_GENERIC_STRUCT)) && ((node_scope)[item] == ns)) && (sym_suffix_equal((node_value)[item], name) == 1))) {
        return item;
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  return 0;
}

int tc_find_enum(int name) {
  int item = (node_a)[tc_root];
  while ((item != 0)) {
    {
      if ((((node_kind)[item] == N_ENUM) && ((node_value)[item] == name))) {
        return item;
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  return 0;
}

int tc_find_enum_ctx(int name, int ns) {
  int exact = tc_find_enum(name);
  if ((exact != 0)) {
    return exact;
  } else {
    {
    }
  }
  if ((ns == 0)) {
    return 0;
  } else {
    {
    }
  }
  int item = (node_a)[tc_root];
  while ((item != 0)) {
    {
      if (((((node_kind)[item] == N_ENUM) && ((node_scope)[item] == ns)) && (sym_suffix_equal((node_value)[item], name) == 1))) {
        return item;
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  return 0;
}

int tc_generic_arity(int decl) {
  if (((decl == 0) || ((node_kind)[decl] != N_GENERIC_STRUCT))) {
    return 0;
  } else {
    {
    }
  }
  int n = 0;
  int p = (node_c)[decl];
  while ((p != 0)) {
    {
      n = (n + 1);
      p = (node_next)[p];
    }
  }
  return n;
}

int tc_generic_arg_count(int ty) {
  if (((ty == 0) || ((node_kind)[ty] != TY_GENERIC))) {
    return 0;
  } else {
    {
    }
  }
  int n = 0;
  int p = (node_a)[ty];
  while ((p != 0)) {
    {
      n = (n + 1);
      p = (node_next)[p];
    }
  }
  return n;
}

int tc_named_exists_ctx(int name, int ns) {
  if ((tc_find_struct_ctx(name, ns) != 0)) {
    return 1;
  } else {
    {
    }
  }
  if ((tc_find_enum_ctx(name, ns) != 0)) {
    return 1;
  } else {
    {
    }
  }
  return 0;
}

int tc_named_exists(int name) {
  if ((tc_find_struct(name) != 0)) {
    return 1;
  } else {
    {
    }
  }
  if ((tc_find_enum(name) != 0)) {
    return 1;
  } else {
    {
    }
  }
  return 0;
}

void tc_check_type(int ty) {
  if ((ty == 0)) {
    {
      tc_fail(1);
      return;
    }
  } else {
    {
    }
  }
  int k = (node_kind)[ty];
  if ((k == TY_NAMED)) {
    {
      if ((tc_named_exists_ctx((node_value)[ty], (node_scope)[ty]) == 0)) {
        tc_fail(2);
      } else {
        {
        }
      }
    }
  } else {
    if ((k == TY_GENERIC)) {
      {
        int s = tc_find_struct_ctx((node_value)[ty], (node_scope)[ty]);
        if (((s == 0) || ((node_kind)[s] != N_GENERIC_STRUCT))) {
          tc_fail(2);
        } else {
          {
            if ((tc_generic_arity(s) != tc_generic_arg_count(ty))) {
              tc_fail(37);
            } else {
              {
              }
            }
            int a = (node_a)[ty];
            while ((a != 0)) {
              {
                tc_check_type(a);
                a = (node_next)[a];
              }
            }
          }
        }
      }
    } else {
      if ((k == TY_PTR)) {
        tc_check_type((node_a)[ty]);
      } else {
        if ((k == TY_ARRAY)) {
          tc_check_type((node_a)[ty]);
        } else {
          if ((k == TY_DYN_ARRAY)) {
            tc_check_type((node_a)[ty]);
          } else {
            if ((k == TY_FUN)) {
              {
                int p = (node_a)[ty];
                while ((p != 0)) {
                  {
                    tc_check_type(p);
                    p = (node_next)[p];
                  }
                }
                tc_check_type((node_b)[ty]);
              }
            } else {
              {
              }
            }
          }
        }
      }
    }
  }
}

int tc_cycle_struct(int name) {
  int i = 0;
  while ((i < tc_path_count)) {
    {
      if (((tc_path_name)[i] == name)) {
        return 1;
      } else {
        {
        }
      }
      i = (i + 1);
    }
  }
  int s = tc_find_struct(name);
  if ((s == 0)) {
    return 0;
  } else {
    {
    }
  }
  ensure_tc_path(tc_path_count);
  (tc_path_name)[tc_path_count] = name;
  tc_path_count = (tc_path_count + 1);
  int f = (node_a)[s];
  int bad = 0;
  while ((f != 0)) {
    {
      if ((tc_cycle_type((node_b)[f]) == 1)) {
        bad = 1;
      } else {
        {
        }
      }
      f = (node_next)[f];
    }
  }
  tc_path_count = (tc_path_count - 1);
  return bad;
}

int tc_cycle_type(int ty) {
  if ((ty == 0)) {
    return 0;
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_PTR)) {
    return 0;
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_FUN)) {
    return 0;
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_ARRAY)) {
    return tc_cycle_type((node_a)[ty]);
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_DYN_ARRAY)) {
    return 0;
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_NAMED)) {
    return tc_cycle_struct((node_value)[ty]);
  } else {
    {
    }
  }
  if (((node_kind)[ty] == TY_GENERIC)) {
    return 0;
  } else {
    {
    }
  }
  return 0;
}

int tc_name_is_array_free(int name) {
  if (((sym_len)[name] != 10)) {
    return 0;
  } else {
    {
    }
  }
  int s = (sym_start)[name];
  if ((((((source)[s] != 97) || ((source)[(s + 1)] != 114)) || ((source)[(s + 2)] != 114)) || ((source)[(s + 3)] != 97))) {
    return 0;
  } else {
    {
    }
  }
  if ((((((source)[(s + 4)] != 121) || ((source)[(s + 5)] != 95)) || ((source)[(s + 6)] != 102)) || ((source)[(s + 7)] != 114))) {
    return 0;
  } else {
    {
    }
  }
  if ((((source)[(s + 8)] != 101) || ((source)[(s + 9)] != 101))) {
    return 0;
  } else {
    {
    }
  }
  return 1;
}

int tc_release_name(int name) {
  if ((((sym_len)[name] == 9) && ((sym_hash)[name] == 340336))) {
    return 1;
  } else {
    {
    }
  }
  if ((((sym_len)[name] == 10) && ((sym_hash)[name] == 327082))) {
    return 1;
  } else {
    {
    }
  }
  if ((tc_name_is_array_free(name) == 1)) {
    return 1;
  } else {
    {
    }
  }
  return 0;
}

int tc_owned_initializer(int id) {
  if (((id == 0) || ((node_kind)[id] != N_CALL))) {
    return 0;
  } else {
    {
    }
  }
  if ((((sym_len)[(node_value)[id]] == 10) && ((sym_hash)[(node_value)[id]] == 984821))) {
    return 1;
  } else {
    {
    }
  }
  if ((((sym_len)[(node_value)[id]] == 9) && ((sym_hash)[(node_value)[id]] == 17002))) {
    return 1;
  } else {
    {
    }
  }
  if ((((sym_len)[(node_value)[id]] == 21) && ((sym_hash)[(node_value)[id]] == 375664))) {
    return 1;
  } else {
    {
    }
  }
  if ((((sym_len)[(node_value)[id]] == 21) && ((sym_hash)[(node_value)[id]] == 191106))) {
    return 1;
  } else {
    {
    }
  }
  return 0;
}

int tc_is_owner_kind(int kind) {
  if ((kind == TY_DYN_ARRAY)) {
    return 1;
  } else {
    {
    }
  }
  return 0;
}

int tc_borrow_conflict(int index) {
  if ((index < 0)) {
    return 0;
  } else {
    {
    }
  }
  if (((index < tc_var_count) && ((tc_var_borrow_count)[index] > 0))) {
    return 1;
  } else {
    {
    }
  }
  return 0;
}

void tc_move_var(int index) {
  if ((index < 0)) {
    {
      tc_fail(5);
      return;
    }
  } else {
    {
    }
  }
  if ((index < tc_var_count)) {
    {
    }
  } else {
    {
      tc_fail(5);
      return;
    }
  }
  if ((tc_borrow_conflict(index) == 1)) {
    {
      tc_fail(37);
      return;
    }
  } else {
    {
    }
  }
  if (((tc_var_moved)[index] == 1)) {
    {
      tc_fail(34);
      return;
    }
  } else {
    {
    }
  }
  if (((tc_var_owned)[index] == 0)) {
    {
      tc_fail(35);
      return;
    }
  } else {
    {
    }
  }
  (tc_var_moved)[index] = 1;
  (tc_var_owned)[index] = 0;
}

void tc_move_value(int id) {
  if (((id == 0) || ((node_kind)[id] != N_VAR))) {
    return;
  } else {
    {
    }
  }
  if ((tc_lookup_var((node_value)[id]) == 0)) {
    {
      tc_fail(5);
      return;
    }
  } else {
    {
    }
  }
  if ((tc_is_owner_kind(tc_kind) == 1)) {
    tc_move_var(tc_last_var_index);
  } else {
    {
    }
  }
}

void tc_record_borrow(int destination, int source_index2) {
  if (((destination < 0) || (source_index2 < 0))) {
    return;
  } else {
    {
    }
  }
  if ((destination < tc_var_count)) {
    {
    }
  } else {
    return;
  }
  if ((source_index2 < tc_var_count)) {
    {
    }
  } else {
    return;
  }
  if (((tc_var_moved)[source_index2] == 1)) {
    {
      tc_fail(33);
      return;
    }
  } else {
    {
    }
  }
  (tc_var_borrow_source)[destination] = source_index2;
  (tc_var_borrow_count)[source_index2] = ((tc_var_borrow_count)[source_index2] + 1);
}

void tc_require_mutable(int id) {
  if ((id == 0)) {
    return;
  } else {
    {
    }
  }
  if (((node_kind)[id] == N_VAR)) {
    {
      if (((tc_lookup_var((node_value)[id]) == 1) && (tc_borrow_conflict(tc_last_var_index) == 1))) {
        tc_fail(37);
      } else {
        {
        }
      }
    }
  } else {
    if (((((node_kind)[id] == N_DEREF) || ((node_kind)[id] == N_INDEX)) || ((node_kind)[id] == N_FIELD_ACCESS))) {
      {
        tc_expr(id);
        if ((tc_expr_borrow_source < 0)) {
          return;
        } else {
          {
          }
        }
        tc_fail(37);
      }
    } else {
      {
      }
    }
  }
}

void tc_consume_call(int id) {
  if ((((id == 0) || ((node_kind)[id] != N_CALL)) || (tc_release_name((node_value)[id]) == 0))) {
    return;
  } else {
    {
    }
  }
  int arg = (node_a)[id];
  if (((arg == 0) || ((node_kind)[arg] != N_VAR))) {
    return;
  } else {
    {
    }
  }
  if ((tc_lookup_var((node_value)[arg]) == 0)) {
    {
      tc_fail(5);
      return;
    }
  } else {
    {
    }
  }
  if ((tc_last_var_moved == 1)) {
    {
      tc_fail(34);
      return;
    }
  } else {
    {
    }
  }
  if ((tc_last_var_owned == 0)) {
    {
      tc_fail(35);
      return;
    }
  } else {
    {
    }
  }
  if ((tc_borrow_conflict(tc_last_var_index) == 1)) {
    {
      tc_fail(37);
      return;
    }
  } else {
    {
    }
  }
  (tc_var_moved)[tc_last_var_index] = 1;
  (tc_var_owned)[tc_last_var_index] = 0;
}

void tc_add_var(int name, int kind, int named, int elem_kind, int elem_name, int type_node) {
  int begin = 0;
  if ((tc_scope_count > 0)) {
    begin = (tc_scope_start)[(tc_scope_count - 1)];
  } else {
    {
    }
  }
  int i = begin;
  while ((i < tc_var_count)) {
    {
      if (((tc_var_name)[i] == name)) {
        {
          tc_fail(3);
          return;
        }
      } else {
        {
        }
      }
      i = (i + 1);
    }
  }
  ensure_tc_vars(tc_var_count);
  (tc_var_name)[tc_var_count] = name;
  (tc_var_kind)[tc_var_count] = kind;
  (tc_var_named)[tc_var_count] = named;
  (tc_var_elem_kind)[tc_var_count] = elem_kind;
  (tc_var_elem_name)[tc_var_count] = elem_name;
  (tc_var_type)[tc_var_count] = type_node;
  (tc_var_owned)[tc_var_count] = 0;
  (tc_var_moved)[tc_var_count] = 0;
  (tc_var_borrow_count)[tc_var_count] = 0;
  (tc_var_borrow_source)[tc_var_count] = (0 - 1);
  tc_last_var_index = tc_var_count;
  (sym_type)[name] = kind;
  (sym_elem_kind)[name] = elem_kind;
  (sym_elem_name)[name] = elem_name;
  tc_var_count = (tc_var_count + 1);
}

int tc_lookup_var(int name) {
  tc_last_var_type = 0;
  tc_last_var_owned = 0;
  tc_last_var_moved = 0;
  tc_expr_borrow_source = (0 - 1);
  tc_last_var_index = 0;
  int i = (tc_var_count - 1);
  while ((1 == 1)) {
    {
      if ((i < 0)) {
        return 0;
      } else {
        {
        }
      }
      if (((tc_var_name)[i] == name)) {
        {
          tc_kind = (tc_var_kind)[i];
          tc_name = (tc_var_named)[i];
          tc_elem_kind = (tc_var_elem_kind)[i];
          tc_elem_name = (tc_var_elem_name)[i];
          tc_last_var_type = (tc_var_type)[i];
          if (((tc_kind == TY_DYN_ARRAY) && (tc_last_var_type != 0))) {
            {
              tc_elem_kind = (node_kind)[(node_a)[tc_last_var_type]];
              if ((tc_elem_kind == TY_NAMED)) {
                tc_elem_name = (node_value)[(node_a)[tc_last_var_type]];
              } else {
                {
                }
              }
            }
          } else {
            {
            }
          }
          tc_last_var_owned = (tc_var_owned)[i];
          tc_last_var_moved = (tc_var_moved)[i];
          tc_expr_borrow_source = (tc_var_borrow_source)[i];
          tc_last_var_index = i;
          return 1;
        }
      } else {
        {
        }
      }
      i = (i - 1);
    }
  }
  return 0;
}

void tc_type_node(int ty) {
  tc_check_type(ty);
  tc_result_type = ty;
  tc_elem_kind = 0;
  tc_elem_name = 0;
  if ((ty == 0)) {
    {
      tc_kind = TY_VOID;
      tc_name = 0;
      return;
    }
  } else {
    {
    }
  }
  tc_kind = (node_kind)[ty];
  if ((tc_kind == TY_NAMED)) {
    {
      tc_name = (node_value)[ty];
    }
  } else {
    if (((tc_kind == TY_GENERIC) || (tc_kind == TY_PARAM))) {
      {
        tc_name = ty;
      }
    } else {
      {
        tc_name = 0;
      }
    }
  }
  if (((tc_kind == TY_PTR) && ((node_a)[ty] != 0))) {
    {
      tc_elem_kind = (node_kind)[(node_a)[ty]];
      if ((tc_elem_kind == TY_NAMED)) {
        {
          tc_elem_name = (node_value)[(node_a)[ty]];
        }
      } else {
        if (((tc_elem_kind == TY_GENERIC) || (tc_elem_kind == TY_PARAM))) {
          {
            tc_elem_name = (node_a)[ty];
          }
        } else {
          {
          }
        }
      }
    }
  } else {
    if (((tc_kind == TY_DYN_ARRAY) && ((node_a)[ty] != 0))) {
      {
        tc_elem_kind = (node_kind)[(node_a)[ty]];
        if ((tc_elem_kind == TY_NAMED)) {
          {
            tc_elem_name = (node_value)[(node_a)[ty]];
          }
        } else {
          if (((tc_elem_kind == TY_GENERIC) || (tc_elem_kind == TY_PARAM))) {
            {
              tc_elem_name = (node_a)[ty];
            }
          } else {
            {
            }
          }
        }
      }
    } else {
      {
      }
    }
  }
}

void tc_expr(int id) {
  tc_kind = TY_INT;
  tc_name = 0;
  tc_elem_kind = 0;
  tc_elem_name = 0;
  tc_result_type = 0;
  tc_expr_borrow_source = (0 - 1);
  if ((id != 0)) {
    tc_error_pos = (node_pos)[id];
  } else {
    {
    }
  }
  if ((id == 0)) {
    {
      tc_fail(4);
      return;
    }
  } else {
    {
    }
  }
  int k = (node_kind)[id];
  if ((k == N_INT)) {
    {
      tc_kind = TY_INT;
      tc_result_type = ast_node(TY_INT, 0, 0, 0, 0, 0);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_FLOAT)) {
    {
      tc_kind = TY_DOUBLE;
      tc_result_type = ast_node(TY_DOUBLE, 0, 0, 0, 0, 0);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_CHAR)) {
    {
      tc_kind = TY_CHAR;
      tc_result_type = ast_node(TY_CHAR, 0, 0, 0, 0, 0);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_NULL)) {
    {
      tc_kind = TY_PTR;
      tc_name = 0;
      tc_elem_kind = TY_VOID;
      tc_elem_name = 0;
      tc_result_type = ast_node(TY_PTR, ast_node(TY_VOID, 0, 0, 0, 0, 0), 0, 0, 0, 0);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_BOOL)) {
    {
      tc_kind = TY_BOOL;
      tc_result_type = ast_node(TY_BOOL, 0, 0, 0, 0, 0);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_STRING)) {
    {
      tc_kind = TY_STRING;
      tc_result_type = ast_node(TY_STRING, 0, 0, 0, 0, 0);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_VAR)) {
    {
      if ((tc_lookup_var((node_value)[id]) == 1)) {
        {
          if ((tc_last_var_moved == 1)) {
            tc_fail(33);
          } else {
            {
            }
          }
          tc_expr_borrow_source = (tc_var_borrow_source)[tc_last_var_index];
          tc_result_type = tc_last_var_type;
          (node_aux)[id] = tc_last_var_type;
          return;
        }
      } else {
        {
        }
      }
      int e = tc_find_enum_value((node_value)[id]);
      if ((e != 0)) {
        {
          tc_kind = TY_NAMED;
          tc_name = e;
          return;
        }
      } else {
        {
        }
      }
      tc_error_symbol = (node_value)[id];
      tc_fail(5);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_ADDRESS)) {
    {
      if ((((node_kind)[(node_a)[id]] == N_VAR) && (tc_find_function_ctx((node_value)[(node_a)[id]], (node_scope)[(node_a)[id]]) != 0))) {
        {
          tc_kind = TY_FUN;
          tc_name = 0;
          return;
        }
      } else {
        {
        }
      }
      tc_expr((node_a)[id]);
      int oldk = tc_kind;
      int oldn = tc_name;
      int olde = tc_elem_kind;
      int olden = tc_elem_name;
      if ((((node_kind)[(node_a)[id]] == N_VAR) && (tc_lookup_var((node_value)[(node_a)[id]]) == 1))) {
        tc_expr_borrow_source = tc_last_var_index;
      } else {
        {
        }
      }
      tc_kind = TY_PTR;
      tc_name = oldn;
      tc_elem_kind = oldk;
      tc_elem_name = oldn;
      if ((oldk == TY_PTR)) {
        {
          tc_elem_kind = olde;
          tc_elem_name = olden;
        }
      } else {
        {
        }
      }
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_DEREF)) {
    {
      tc_expr((node_a)[id]);
      int deref_borrow = tc_expr_borrow_source;
      if ((tc_kind != TY_PTR)) {
        tc_fail(6);
      } else {
        {
          tc_kind = tc_elem_kind;
          tc_name = tc_elem_name;
          tc_elem_kind = 0;
          tc_elem_name = 0;
          tc_expr_borrow_source = deref_borrow;
        }
      }
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_INDEX)) {
    {
      tc_expr((node_b)[id]);
      if ((tc_kind != TY_INT)) {
        tc_fail(7);
      } else {
        {
        }
      }
      tc_expr((node_a)[id]);
      int index_borrow = tc_expr_borrow_source;
      if ((tc_kind == TY_ARRAY)) {
        {
          tc_kind = TY_INT;
          tc_name = 0;
          tc_elem_kind = 0;
          tc_elem_name = 0;
          tc_expr_borrow_source = (0 - 1);
        }
      } else {
        if ((tc_kind == TY_DYN_ARRAY)) {
          {
            int ek = tc_elem_kind;
            int en = tc_elem_name;
            tc_kind = ek;
            tc_name = en;
            tc_elem_kind = 0;
            tc_elem_name = 0;
            tc_expr_borrow_source = index_borrow;
          }
        } else {
          if ((tc_kind == TY_PTR)) {
            {
              tc_kind = tc_elem_kind;
              tc_name = tc_elem_name;
              tc_elem_kind = 0;
              tc_elem_name = 0;
              tc_expr_borrow_source = index_borrow;
            }
          } else {
            if ((tc_kind == TY_STRING)) {
              {
                tc_kind = TY_CHAR;
                tc_name = 0;
                tc_elem_kind = 0;
                tc_elem_name = 0;
                tc_expr_borrow_source = (0 - 1);
              }
            } else {
              tc_fail(8);
            }
          }
        }
      }
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_FIELD_ACCESS)) {
    {
      tc_expr((node_a)[id]);
      int base_kind = tc_kind;
      int base_name = tc_name;
      if ((base_kind == TY_DYN_ARRAY)) {
        {
          if ((((sym_len)[(node_value)[id]] == 3) && ((sym_hash)[(node_value)[id]] == 315566))) {
            {
              tc_kind = TY_INT;
              tc_name = 0;
              tc_elem_kind = 0;
              tc_elem_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 3) && ((sym_hash)[(node_value)[id]] == 306795))) {
            {
              tc_kind = TY_INT;
              tc_name = 0;
              tc_elem_kind = 0;
              tc_elem_name = 0;
              return;
            }
          } else {
            {
            }
          }
          tc_fail(11);
          return;
        }
      } else {
        {
        }
      }
      if ((base_kind == TY_GENERIC)) {
        {
          int base_ty = base_name;
          int sgen = tc_find_struct((node_value)[base_ty]);
          if ((sgen == 0)) {
            {
              tc_fail(10);
              return;
            }
          } else {
            {
            }
          }
          tc_bind_clear();
          int gp = (node_c)[sgen];
          int ga = (node_a)[base_ty];
          while (((gp != 0) && (ga != 0))) {
            {
              if ((tc_bind_add((node_a)[gp], ga) == 0)) {
                return;
              } else {
                {
                }
              }
              gp = (node_next)[gp];
              ga = (node_next)[ga];
            }
          }
          int gf = (node_a)[sgen];
          while ((gf != 0)) {
            {
              if (((node_a)[gf] == (node_value)[id])) {
                {
                  int subst = tc_substitute_type((node_b)[gf]);
                  tc_type_node(subst);
                  return;
                }
              } else {
                {
                }
              }
              gf = (node_next)[gf];
            }
          }
          tc_fail(11);
          return;
        }
      } else {
        {
        }
      }
      if ((base_kind == TY_PTR)) {
        base_kind = TY_NAMED;
      } else {
        {
        }
      }
      if ((base_kind != TY_NAMED)) {
        {
          tc_fail(9);
          return;
        }
      } else {
        {
        }
      }
      int s = tc_find_struct(base_name);
      if ((s == 0)) {
        {
          tc_fail(10);
          return;
        }
      } else {
        {
        }
      }
      int f = (node_a)[s];
      while ((f != 0)) {
        {
          if (((node_a)[f] == (node_value)[id])) {
            {
              tc_type_node((node_b)[f]);
              return;
            }
          } else {
            {
            }
          }
          f = (node_next)[f];
        }
      }
      tc_fail(11);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_CALL)) {
    {
      int call_name = (node_value)[id];
      int call_len = (sym_len)[call_name];
      int call_hash = (sym_hash)[call_name];
      if (((call_len == 10) && (call_hash == 790299))) {
        {
          int aa = (node_a)[id];
          if (((aa == 0) || ((node_next)[aa] != 0))) {
            {
              tc_fail(13);
              return;
            }
          } else {
            {
            }
          }
          tc_expr(aa);
          if ((((tc_kind != TY_INT) && (tc_kind != TY_BOOL)) && (tc_kind != TY_CHAR))) {
            tc_fail(17);
          } else {
            {
            }
          }
          tc_kind = TY_DYN_ARRAY;
          tc_name = 0;
          tc_elem_kind = TY_INT;
          tc_elem_name = 0;
          return;
        }
      } else {
        {
        }
      }
      if (((call_len == 13) && (call_hash == 333999))) {
        {
          int aa = (node_a)[id];
          if ((((aa == 0) || ((node_next)[aa] == 0)) || ((node_next)[(node_next)[aa]] != 0))) {
            {
              tc_fail(13);
              return;
            }
          } else {
            {
            }
          }
          tc_expr(aa);
          if ((tc_kind != TY_DYN_ARRAY)) {
            {
              tc_fail(8);
              return;
            }
          } else {
            {
            }
          }
          tc_require_mutable(aa);
          int ek = tc_elem_kind;
          int en = tc_elem_name;
          aa = (node_next)[aa];
          tc_expr(aa);
          if (((((((((tc_kind != TY_INT) && (tc_kind != TY_BOOL)) && (tc_kind != TY_CHAR)) && (tc_kind != TY_FLOAT)) && (tc_kind != TY_DOUBLE)) && (tc_kind != TY_STRING)) && (tc_kind != TY_PTR)) && (tc_kind != TY_NAMED))) {
            tc_fail(17);
          } else {
            {
            }
          }
          tc_kind = TY_DYN_ARRAY;
          tc_name = 0;
          tc_elem_kind = ek;
          tc_elem_name = en;
          return;
        }
      } else {
        {
        }
      }
      if (((call_len == 10) && (call_hash == 899143))) {
        {
          int aa = (node_a)[id];
          if ((((aa == 0) || ((node_next)[aa] == 0)) || ((node_next)[(node_next)[aa]] != 0))) {
            {
              tc_fail(13);
              return;
            }
          } else {
            {
            }
          }
          tc_expr(aa);
          if ((tc_kind != TY_DYN_ARRAY)) {
            {
              tc_fail(8);
              return;
            }
          } else {
            {
            }
          }
          tc_require_mutable(aa);
          int ek = tc_elem_kind;
          int en = tc_elem_name;
          aa = (node_next)[aa];
          tc_expr(aa);
          if ((tc_array_elem_same(ek, en, tc_kind, tc_name) == 0)) {
            tc_fail(36);
          } else {
            {
            }
          }
          tc_kind = TY_DYN_ARRAY;
          tc_name = 0;
          tc_elem_kind = ek;
          tc_elem_name = en;
          return;
        }
      } else {
        {
        }
      }
      if (((call_len == 9) && (call_hash == 890825))) {
        {
          int aa = (node_a)[id];
          if ((((aa == 0) || ((node_next)[aa] == 0)) || ((node_next)[(node_next)[aa]] != 0))) {
            {
              tc_fail(13);
              return;
            }
          } else {
            {
            }
          }
          tc_expr(aa);
          if ((tc_kind != TY_DYN_ARRAY)) {
            {
              tc_fail(8);
              return;
            }
          } else {
            {
            }
          }
          int ek = tc_elem_kind;
          int en = tc_elem_name;
          aa = (node_next)[aa];
          tc_expr(aa);
          if (((((((((tc_kind != TY_INT) && (tc_kind != TY_BOOL)) && (tc_kind != TY_CHAR)) && (tc_kind != TY_FLOAT)) && (tc_kind != TY_DOUBLE)) && (tc_kind != TY_STRING)) && (tc_kind != TY_PTR)) && (tc_kind != TY_NAMED))) {
            tc_fail(17);
          } else {
            {
            }
          }
          tc_kind = ek;
          tc_name = en;
          tc_elem_kind = 0;
          tc_elem_name = 0;
          if (((((ek == TY_PTR) && (tc_last_var_type != 0)) && ((node_a)[tc_last_var_type] != 0)) && ((node_a)[(node_a)[tc_last_var_type]] != 0))) {
            {
              int pointee = (node_a)[(node_a)[tc_last_var_type]];
              tc_elem_kind = (node_kind)[pointee];
              if ((tc_elem_kind == TY_NAMED)) {
                tc_elem_name = (node_value)[pointee];
              } else {
                {
                }
              }
            }
          } else {
            {
            }
          }
          return;
        }
      } else {
        {
        }
      }
      if (((call_len == 9) && (call_hash == 902357))) {
        {
          int aa = (node_a)[id];
          if (((((aa == 0) || ((node_next)[aa] == 0)) || ((node_next)[(node_next)[aa]] == 0)) || ((node_next)[(node_next)[(node_next)[aa]]] != 0))) {
            {
              tc_fail(13);
              return;
            }
          } else {
            {
            }
          }
          tc_expr(aa);
          if ((tc_kind != TY_DYN_ARRAY)) {
            {
              tc_fail(8);
              return;
            }
          } else {
            {
            }
          }
          tc_require_mutable(aa);
          int ek = tc_elem_kind;
          int en = tc_elem_name;
          aa = (node_next)[aa];
          tc_expr(aa);
          if (((((((((tc_kind != TY_INT) && (tc_kind != TY_BOOL)) && (tc_kind != TY_CHAR)) && (tc_kind != TY_FLOAT)) && (tc_kind != TY_DOUBLE)) && (tc_kind != TY_STRING)) && (tc_kind != TY_PTR)) && (tc_kind != TY_NAMED))) {
            {
              tc_fail(17);
              return;
            }
          } else {
            {
            }
          }
          aa = (node_next)[aa];
          tc_expr(aa);
          if ((tc_array_elem_same(ek, en, tc_kind, tc_name) == 0)) {
            tc_fail(36);
          } else {
            {
            }
          }
          tc_kind = TY_VOID;
          tc_name = 0;
          tc_elem_kind = 0;
          tc_elem_name = 0;
          return;
        }
      } else {
        {
        }
      }
      if (((call_len == 11) && (call_hash == 585984))) {
        {
          int aa = (node_a)[id];
          if (((aa == 0) || ((node_next)[aa] != 0))) {
            {
              tc_fail(13);
              return;
            }
          } else {
            {
            }
          }
          tc_expr(aa);
          if ((tc_kind != TY_DYN_ARRAY)) {
            {
              tc_fail(8);
              return;
            }
          } else {
            {
            }
          }
          tc_require_mutable(aa);
          int ek = tc_elem_kind;
          int en = tc_elem_name;
          tc_kind = TY_DYN_ARRAY;
          tc_name = 0;
          tc_elem_kind = ek;
          tc_elem_name = en;
          return;
        }
      } else {
        {
        }
      }
      if (((call_len == 10) && (call_hash == 597913))) {
        {
          int aa = (node_a)[id];
          if (((aa == 0) || ((node_next)[aa] != 0))) {
            {
              tc_fail(13);
              return;
            }
          } else {
            {
            }
          }
          tc_expr(aa);
          if ((tc_kind != TY_DYN_ARRAY)) {
            tc_fail(8);
          } else {
            {
            }
          }
          tc_require_mutable(aa);
          tc_kind = TY_VOID;
          tc_name = 0;
          tc_elem_kind = 0;
          tc_elem_name = 0;
          return;
        }
      } else {
        {
        }
      }
      int fun_node = tc_find_function_ctx((node_value)[id], (node_scope)[id]);
      if ((fun_node == 0)) {
        {
          if (((tc_lookup_var((node_value)[id]) == 1) && (tc_kind == TY_FUN))) {
            {
              int fty = tc_last_var_type;
              if (((fty == 0) || ((node_kind)[fty] != TY_FUN))) {
                {
                  tc_kind = TY_INT;
                  tc_name = 0;
                  return;
                }
              } else {
                {
                }
              }
              int arg_fp = (node_a)[id];
              int p_fp = (node_a)[fty];
              while (((arg_fp != 0) && (p_fp != 0))) {
                {
                  tc_expr(arg_fp);
                  int ak_fp = tc_kind;
                  int an_fp = tc_name;
                  int aek_fp = tc_elem_kind;
                  int aen_fp = tc_elem_name;
                  tc_type_node(p_fp);
                  int pek_fp = tc_kind;
                  int pen_fp = tc_name;
                  int peek_fp = tc_elem_kind;
                  int peen_fp = tc_elem_name;
                  if (((pek_fp == TY_DYN_ARRAY) && (ak_fp == TY_DYN_ARRAY))) {
                    tc_move_value(arg_fp);
                  } else {
                    {
                    }
                  }
                  if ((tc_same_full(ak_fp, an_fp, aek_fp, aen_fp, pek_fp, pen_fp, peek_fp, peen_fp) == 0)) {
                    tc_fail(12);
                  } else {
                    {
                    }
                  }
                  arg_fp = (node_next)[arg_fp];
                  p_fp = (node_next)[p_fp];
                }
              }
              if (((arg_fp != 0) || (p_fp != 0))) {
                tc_fail(13);
              } else {
                {
                }
              }
              tc_type_node((node_b)[fty]);
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 10) && ((sym_hash)[(node_value)[id]] == 984821))) {
            {
              tc_kind = TY_PTR;
              tc_name = 0;
              tc_elem_kind = TY_INT;
              tc_elem_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 9) && ((sym_hash)[(node_value)[id]] == 340336))) {
            {
              tc_kind = TY_VOID;
              tc_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 9) && ((sym_hash)[(node_value)[id]] == 739305))) {
            {
              tc_kind = TY_PTR;
              tc_name = 0;
              tc_elem_kind = TY_INT;
              tc_elem_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 9) && ((sym_hash)[(node_value)[id]] == 776147))) {
            {
              tc_kind = TY_STRING;
              tc_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 9) && ((sym_hash)[(node_value)[id]] == 17002))) {
            {
              tc_kind = TY_PTR;
              tc_name = 0;
              tc_elem_kind = TY_VOID;
              tc_elem_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 9) && ((sym_hash)[(node_value)[id]] == 977208))) {
            {
              tc_kind = TY_INT;
              tc_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 10) && ((sym_hash)[(node_value)[id]] == 327082))) {
            {
              tc_kind = TY_INT;
              tc_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 10) && ((sym_hash)[(node_value)[id]] == 493501))) {
            {
              tc_kind = TY_INT;
              tc_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 21) && ((sym_hash)[(node_value)[id]] == 375664))) {
            {
              tc_kind = TY_PTR;
              tc_name = 0;
              tc_elem_kind = TY_VOID;
              tc_elem_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 21) && ((sym_hash)[(node_value)[id]] == 191106))) {
            {
              tc_kind = TY_PTR;
              tc_name = 0;
              tc_elem_kind = TY_VOID;
              tc_elem_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 23) && ((sym_hash)[(node_value)[id]] == 702900))) {
            {
              tc_kind = TY_INT;
              tc_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 17) && ((sym_hash)[(node_value)[id]] == 344177))) {
            {
              tc_kind = TY_VOID;
              tc_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 25) && ((sym_hash)[(node_value)[id]] == 522527))) {
            {
              tc_kind = TY_VOID;
              tc_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 21) && ((sym_hash)[(node_value)[id]] == 686023))) {
            {
              tc_kind = TY_INT;
              tc_name = 0;
              return;
            }
          } else {
            {
            }
          }
          if ((((sym_len)[(node_value)[id]] == 16) && ((sym_hash)[(node_value)[id]] == 904440))) {
            {
              tc_kind = TY_STRING;
              tc_name = 0;
              return;
            }
          } else {
            {
            }
          }
          tc_kind = TY_INT;
          tc_name = 0;
          return;
        }
      } else {
        {
        }
      }
      if (((node_kind)[fun_node] == N_GENERIC_FUNC)) {
        {
          tc_bind_clear();
          int ga = (node_a)[id];
          int gp = (node_c)[fun_node];
          while (((ga != 0) && (gp != 0))) {
            {
              tc_expr(ga);
              int actual_kind = tc_kind;
              int actual_ty = tc_result_type;
              if ((actual_kind == TY_DYN_ARRAY)) {
                tc_move_value(ga);
              } else {
                {
                }
              }
              if ((actual_ty == 0)) {
                actual_ty = tc_type_node_from_summary(tc_kind, tc_name, tc_elem_kind, tc_elem_name);
              } else {
                {
                }
              }
              tc_match_generic((node_b)[gp], actual_ty);
              ga = (node_next)[ga];
              gp = (node_next)[gp];
            }
          }
          if (((ga != 0) || (gp != 0))) {
            tc_fail(13);
          } else {
            {
            }
          }
          int generic_ret = tc_substitute_type((node_b)[fun_node]);
          tc_type_node(generic_ret);
          return;
        }
      } else {
        {
        }
      }
      int arg = (node_a)[id];
      int p = (node_c)[fun_node];
      while (((arg != 0) && (p != 0))) {
        {
          tc_expr(arg);
          int ak = tc_kind;
          int an = tc_name;
          int aek = tc_elem_kind;
          int aen = tc_elem_name;
          tc_type_node((node_b)[p]);
          int pek = tc_kind;
          int pen = tc_name;
          int peek = tc_elem_kind;
          int peen = tc_elem_name;
          if (((pek == TY_DYN_ARRAY) && (ak == TY_DYN_ARRAY))) {
            tc_move_value(arg);
          } else {
            {
            }
          }
          if ((tc_same_full(ak, an, aek, aen, pek, pen, peek, peen) == 0)) {
            tc_fail(12);
          } else {
            {
            }
          }
          arg = (node_next)[arg];
          p = (node_next)[p];
        }
      }
      if (((arg != 0) || (p != 0))) {
        tc_fail(13);
      } else {
        {
        }
      }
      tc_type_node((node_b)[fun_node]);
      return;
    }
  } else {
    {
    }
  }
  if ((k == N_BINOP)) {
    {
      tc_expr((node_a)[id]);
      int ak = tc_kind;
      int an = tc_name;
      int ae = tc_elem_kind;
      int aen = tc_elem_name;
      tc_expr((node_b)[id]);
      int bk = tc_kind;
      int bn = tc_name;
      int be = tc_elem_kind;
      int ben = tc_elem_name;
      if (((node_value)[id] == OP_CONCAT)) {
        {
          if (((ak != TY_STRING) || (bk != TY_STRING))) {
            tc_fail(14);
          } else {
            {
            }
          }
          tc_kind = TY_STRING;
          tc_name = 0;
        }
      } else {
        if ((((node_value)[id] == OP_AND) || ((node_value)[id] == OP_OR))) {
          {
            if ((((ak != TY_BOOL) && (ak != TY_INT)) || ((bk != TY_BOOL) && (bk != TY_INT)))) {
              tc_fail(15);
            } else {
              {
              }
            }
            tc_kind = TY_BOOL;
            tc_name = 0;
          }
        } else {
          if (((((((node_value)[id] == OP_BITAND) || ((node_value)[id] == OP_BITOR)) || ((node_value)[id] == OP_BITXOR)) || ((node_value)[id] == OP_SHL)) || ((node_value)[id] == OP_SHR))) {
            {
              if ((((ak != TY_INT) && (ak != TY_CHAR)) || ((bk != TY_INT) && (bk != TY_CHAR)))) {
                tc_fail(32);
              } else {
                {
                }
              }
              tc_kind = TY_INT;
              tc_name = 0;
            }
          } else {
            if ((((node_value)[id] == OP_EQ) || ((node_value)[id] == OP_NEQ))) {
              {
                int null_cmp = 0;
                if (((((ak == TY_PTR) && (bk == TY_INT)) && ((node_kind)[(node_b)[id]] == N_INT)) && ((node_value)[(node_b)[id]] == 0))) {
                  null_cmp = 1;
                } else {
                  {
                  }
                }
                if (((((ak == TY_INT) && (bk == TY_PTR)) && ((node_kind)[(node_a)[id]] == N_INT)) && ((node_value)[(node_a)[id]] == 0))) {
                  null_cmp = 1;
                } else {
                  {
                  }
                }
                if (((tc_same_full(ak, an, ae, aen, bk, bn, be, ben) == 0) && (null_cmp == 0))) {
                  tc_fail(16);
                } else {
                  {
                  }
                }
                tc_kind = TY_BOOL;
                tc_name = 0;
              }
            } else {
              if ((((node_value)[id] == OP_LT) || ((node_value)[id] == OP_GT))) {
                {
                  if (((((ak != TY_INT) && (ak != TY_FLOAT)) && (ak != TY_DOUBLE)) || (((bk != TY_INT) && (bk != TY_FLOAT)) && (bk != TY_DOUBLE)))) {
                    tc_fail(17);
                  } else {
                    {
                    }
                  }
                  tc_kind = TY_BOOL;
                  tc_name = 0;
                }
              } else {
                if (((node_value)[id] == OP_ADD)) {
                  {
                    if ((((ak == TY_PTR) && (bk == TY_INT)) && (ae != TY_VOID))) {
                      {
                        tc_kind = TY_PTR;
                        tc_name = an;
                        tc_elem_kind = ae;
                        tc_elem_name = aen;
                      }
                    } else {
                      if ((((ak == TY_INT) && (bk == TY_PTR)) && (be != TY_VOID))) {
                        {
                          tc_kind = TY_PTR;
                          tc_name = bn;
                          tc_elem_kind = be;
                          tc_elem_name = ben;
                        }
                      } else {
                        if ((((ak == TY_FLOAT) || (ak == TY_DOUBLE)) && ((bk == TY_FLOAT) || (bk == TY_DOUBLE)))) {
                          {
                            if (((ak == TY_DOUBLE) || (bk == TY_DOUBLE))) {
                              tc_kind = TY_DOUBLE;
                            } else {
                              tc_kind = TY_FLOAT;
                            }
                            tc_name = 0;
                          }
                        } else {
                          if (((ak == TY_INT) && (bk == TY_INT))) {
                            {
                              tc_kind = TY_INT;
                              tc_name = 0;
                            }
                          } else {
                            tc_fail(18);
                          }
                        }
                      }
                    }
                  }
                } else {
                  if (((node_value)[id] == OP_SUB)) {
                    {
                      if ((((ak == TY_PTR) && (bk == TY_INT)) && (ae != TY_VOID))) {
                        {
                          tc_kind = TY_PTR;
                          tc_name = an;
                          tc_elem_kind = ae;
                          tc_elem_name = aen;
                        }
                      } else {
                        if ((tc_ptr_diff_ok(ak, ae, aen, bk, be, ben) == 1)) {
                          {
                            tc_kind = TY_INT;
                            tc_name = 0;
                            tc_elem_kind = 0;
                            tc_elem_name = 0;
                          }
                        } else {
                          if ((((ak == TY_FLOAT) || (ak == TY_DOUBLE)) && ((bk == TY_FLOAT) || (bk == TY_DOUBLE)))) {
                            {
                              if (((ak == TY_DOUBLE) || (bk == TY_DOUBLE))) {
                                tc_kind = TY_DOUBLE;
                              } else {
                                tc_kind = TY_FLOAT;
                              }
                              tc_name = 0;
                            }
                          } else {
                            if (((ak == TY_INT) && (bk == TY_INT))) {
                              {
                                tc_kind = TY_INT;
                                tc_name = 0;
                              }
                            } else {
                              tc_fail(18);
                            }
                          }
                        }
                      }
                    }
                  } else {
                    if (((((node_value)[id] == OP_MUL) || ((node_value)[id] == OP_DIV)) || ((node_value)[id] == OP_MOD))) {
                      {
                        if (((ak == TY_DOUBLE) || (bk == TY_DOUBLE))) {
                          tc_kind = TY_DOUBLE;
                        } else {
                          if (((ak == TY_FLOAT) || (bk == TY_FLOAT))) {
                            tc_kind = TY_FLOAT;
                          } else {
                            {
                              if (((ak != TY_INT) || (bk != TY_INT))) {
                                tc_fail(18);
                              } else {
                                {
                                }
                              }
                              tc_kind = TY_INT;
                            }
                          }
                        }
                        tc_name = 0;
                      }
                    } else {
                      {
                        tc_fail(18);
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
      return;
    }
  } else {
    {
    }
  }
  tc_fail(19);
}

int tc_find_function(int name) {
  int item = (node_a)[tc_root];
  while ((item != 0)) {
    {
      if ((((((node_kind)[item] == N_FUNC) || ((node_kind)[item] == N_GENERIC_FUNC)) || ((node_kind)[item] == N_EXTERN)) && ((node_value)[item] == name))) {
        return item;
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  return 0;
}

int tc_find_function_ctx(int name, int ns) {
  int exact = tc_find_function(name);
  if ((exact != 0)) {
    return exact;
  } else {
    {
    }
  }
  if ((ns == 0)) {
    return 0;
  } else {
    {
    }
  }
  int item = (node_a)[tc_root];
  while ((item != 0)) {
    {
      if (((((((node_kind)[item] == N_FUNC) || ((node_kind)[item] == N_GENERIC_FUNC)) || ((node_kind)[item] == N_EXTERN)) && ((node_scope)[item] == ns)) && (sym_suffix_equal((node_value)[item], name) == 1))) {
        return item;
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  return 0;
}

int tc_find_enum_value(int name) {
  int item = (node_a)[tc_root];
  while ((item != 0)) {
    {
      if (((node_kind)[item] == N_ENUM)) {
        {
          int f = (node_a)[item];
          while ((f != 0)) {
            {
              if (((node_a)[f] == name)) {
                return (node_value)[item];
              } else {
                {
                }
              }
              f = (node_next)[f];
            }
          }
        }
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  return 0;
}

int tc_emit_arg_type(int id) {
  if ((id == 0)) {
    return 0;
  } else {
    {
    }
  }
  if ((((node_kind)[id] == N_VAR) && ((node_aux)[id] != 0))) {
    return (node_aux)[id];
  } else {
    {
    }
  }
  if (((node_kind)[id] == N_STRING)) {
    return ast_node(TY_STRING, 0, 0, 0, 0, 0);
  } else {
    {
    }
  }
  if (((node_kind)[id] == N_INT)) {
    return ast_node(TY_INT, 0, 0, 0, 0, 0);
  } else {
    {
    }
  }
  if (((node_kind)[id] == N_BOOL)) {
    return ast_node(TY_BOOL, 0, 0, 0, 0, 0);
  } else {
    {
    }
  }
  if (((node_kind)[id] == N_CHAR)) {
    return ast_node(TY_CHAR, 0, 0, 0, 0, 0);
  } else {
    {
    }
  }
  if (((node_kind)[id] == N_FLOAT)) {
    return ast_node(TY_DOUBLE, 0, 0, 0, 0, 0);
  } else {
    {
    }
  }
  if (((node_kind)[id] == N_NULL)) {
    return ast_node(TY_PTR, ast_node(TY_VOID, 0, 0, 0, 0, 0), 0, 0, 0, 0);
  } else {
    {
    }
  }
  int ek = gen_expr_kind(id);
  return tc_type_node_from_summary(ek, 0, tc_elem_kind, tc_elem_name);
}

int tc_expr_kind_for_emit(int id) {
  int f = tc_find_function_ctx((node_value)[id], (node_scope)[id]);
  if ((f != 0)) {
    {
      if (((node_kind)[f] == N_GENERIC_FUNC)) {
        {
          int actual = 0;
          int a = (node_a)[id];
          while ((a != 0)) {
            {
              int q = tc_emit_arg_type(a);
              if ((q != 0)) {
                {
                  if ((actual == 0)) {
                    actual = q;
                  } else {
                    actual = ast_link(actual, q);
                  }
                }
              } else {
                {
                }
              }
              a = (node_next)[a];
            }
          }
          gen_bind_decl(f, actual);
          int ret = gen_substitute_type((node_b)[f]);
          int result_kind = TY_INT;
          if ((ret != 0)) {
            {
              result_kind = (node_kind)[ret];
            }
          } else {
            {
            }
          }
          gen_bind_clear();
          if ((result_kind == TY_PARAM)) {
            {
              return TY_INT;
            }
          } else {
            {
            }
          }
          return result_kind;
        }
      } else {
        {
        }
      }
      if (((node_b)[f] != 0)) {
        {
          return (node_kind)[(node_b)[f]];
        }
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  return TY_INT;
}

void tc_stmt(int id, int expected_kind, int expected_name) {
  if ((id != 0)) {
    tc_error_pos = (node_pos)[id];
  } else {
    {
    }
  }
  if (((tc_ok == 0) || (id == 0))) {
    return;
  } else {
    {
    }
  }
  int k = (node_kind)[id];
  if ((k == N_CONST)) {
    {
      tc_type_node((node_b)[id]);
      int ck = tc_kind;
      int cn = tc_name;
      int ce = tc_elem_kind;
      int cen = tc_elem_name;
      tc_expr((node_c)[id]);
      if ((tc_same_full(ck, cn, ce, cen, tc_kind, tc_name, tc_elem_kind, tc_elem_name) == 0)) {
        {
          if ((((node_kind)[(node_c)[id]] != N_INT) || ((node_value)[(node_c)[id]] != 0))) {
            if (((node_kind)[(node_c)[id]] != N_NULL)) {
              tc_fail(30);
            } else {
              {
              }
            }
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
      tc_add_var((node_a)[id], ck, cn, ce, cen, (node_b)[id]);
      (sym_type)[(node_a)[id]] = (ck + 100);
    }
  } else {
    if ((k == N_LET)) {
      {
        tc_type_node((node_b)[id]);
        int dk = tc_kind;
        int dn = tc_name;
        int de = tc_elem_kind;
        int den = tc_elem_name;
        tc_expr((node_c)[id]);
        int ek = tc_kind;
        int en = tc_name;
        int ee = tc_elem_kind;
        int een = tc_elem_name;
        int rhs_borrow = tc_expr_borrow_source;
        if (((tc_is_owner_kind(dk) == 1) && ((node_kind)[(node_c)[id]] == N_VAR))) {
          tc_fail(40);
        } else {
          {
          }
        }
        if ((tc_same_full(dk, dn, de, den, ek, en, ee, een) == 0)) {
          {
            int polymorphic_make = 0;
            if (((((node_kind)[(node_c)[id]] == N_CALL) && ((sym_len)[(node_value)[(node_c)[id]]] == 10)) && ((sym_hash)[(node_value)[(node_c)[id]]] == 790299))) {
              polymorphic_make = 1;
            } else {
              {
              }
            }
            if ((((polymorphic_make == 0) && (((node_kind)[(node_c)[id]] != N_INT) || ((node_value)[(node_c)[id]] != 0))) && ((node_kind)[(node_c)[id]] != N_NULL))) {
              tc_fail(20);
            } else {
              {
              }
            }
          }
        } else {
          {
          }
        }
        if (((dk == TY_DYN_ARRAY) && ((node_kind)[(node_c)[id]] == N_VAR))) {
          tc_move_value((node_c)[id]);
        } else {
          {
          }
        }
        tc_add_var((node_a)[id], dk, dn, de, den, (node_b)[id]);
        if (((dk == TY_DYN_ARRAY) || (tc_owned_initializer((node_c)[id]) == 1))) {
          (tc_var_owned)[tc_last_var_index] = 1;
        } else {
          {
          }
        }
        if ((dk == TY_PTR)) {
          {
            if ((rhs_borrow < 0)) {
              {
              }
            } else {
              tc_record_borrow(tc_last_var_index, rhs_borrow);
            }
          }
        } else {
          {
          }
        }
      }
    } else {
      if ((k == N_ASSIGN)) {
        {
          if ((((node_kind)[(node_a)[id]] == N_VAR) && ((sym_type)[(node_value)[(node_a)[id]]] > 100))) {
            tc_fail(31);
          } else {
            {
            }
          }
          tc_expr((node_a)[id]);
          int lk = tc_kind;
          int ln = tc_name;
          int le = tc_elem_kind;
          int len = tc_elem_name;
          int lhs_borrow = tc_expr_borrow_source;
          int lhs_index = tc_last_var_index;
          tc_require_mutable((node_a)[id]);
          tc_expr((node_b)[id]);
          int rhs_borrow_assign = tc_expr_borrow_source;
          if (((lk == TY_DYN_ARRAY) && ((node_kind)[(node_b)[id]] == N_VAR))) {
            tc_fail(40);
          } else {
            {
            }
          }
          if ((tc_same_full(lk, ln, le, len, tc_kind, tc_name, tc_elem_kind, tc_elem_name) == 0)) {
            {
              if (((((node_kind)[(node_b)[id]] != N_INT) || ((node_value)[(node_b)[id]] != 0)) && ((node_kind)[(node_b)[id]] != N_NULL))) {
                tc_fail(21);
              } else {
                {
                }
              }
            }
          } else {
            {
            }
          }
          if ((((tc_ok == 1) && ((node_kind)[(node_a)[id]] == N_VAR)) && (lk == TY_PTR))) {
            {
              if ((lhs_borrow < 0)) {
                {
                }
              } else {
                if (((lhs_borrow < tc_var_count) && ((tc_var_borrow_count)[lhs_borrow] > 0))) {
                  (tc_var_borrow_count)[lhs_borrow] = ((tc_var_borrow_count)[lhs_borrow] - 1);
                } else {
                  {
                  }
                }
              }
              (tc_var_borrow_source)[lhs_index] = (0 - 1);
              if ((rhs_borrow_assign < 0)) {
                {
                }
              } else {
                tc_record_borrow(lhs_index, rhs_borrow_assign);
              }
            }
          } else {
            {
            }
          }
        }
      } else {
        if ((k == N_PRINT)) {
          tc_expr((node_a)[id]);
        } else {
          if ((k == N_EXPR)) {
            {
              tc_expr((node_a)[id]);
              if (((node_kind)[(node_a)[id]] == N_CALL)) {
                tc_consume_call((node_a)[id]);
              } else {
                {
                }
              }
            }
          } else {
            if ((k == N_RETURN)) {
              {
                if (((node_a)[id] == 0)) {
                  {
                    if ((expected_kind != TY_VOID)) {
                      tc_fail(22);
                    } else {
                      {
                      }
                    }
                  }
                } else {
                  {
                    tc_expr((node_a)[id]);
                    int return_borrow = tc_expr_borrow_source;
                    if ((expected_kind == TY_PTR)) {
                      {
                        if ((return_borrow < 0)) {
                          {
                          }
                        } else {
                          tc_fail(38);
                        }
                      }
                    } else {
                      {
                      }
                    }
                    if ((tc_same_full(expected_kind, expected_name, tc_expected_elem_kind, tc_expected_elem_name, tc_kind, tc_name, tc_elem_kind, tc_elem_name) == 0)) {
                      if (((node_kind)[(node_a)[id]] != N_NULL)) {
                        tc_fail(23);
                      } else {
                        {
                        }
                      }
                    } else {
                      {
                      }
                    }
                  }
                }
              }
            } else {
              if (((k == N_BREAK) || (k == N_CONTINUE))) {
                {
                  if ((tc_loop_depth == 0)) {
                    tc_fail(24);
                  } else {
                    {
                    }
                  }
                }
              } else {
                if ((k == N_BLOCK)) {
                  {
                    tc_enter_scope();
                    int x = (node_a)[id];
                    while ((x != 0)) {
                      {
                        tc_stmt(x, expected_kind, expected_name);
                        x = (node_next)[x];
                      }
                    }
                    tc_leave_scope();
                  }
                } else {
                  if ((k == N_IF)) {
                    {
                      tc_expr((node_a)[id]);
                      if ((((((((tc_kind != TY_BOOL) && (tc_kind != TY_INT)) && (tc_kind != TY_CHAR)) && (tc_kind != TY_FLOAT)) && (tc_kind != TY_DOUBLE)) && (tc_kind != TY_PTR)) && (tc_kind != TY_FUN))) {
                        tc_fail(25);
                      } else {
                        {
                        }
                      }
                      tc_stmt((node_b)[id], expected_kind, expected_name);
                      tc_stmt((node_c)[id], expected_kind, expected_name);
                    }
                  } else {
                    if ((k == N_WHILE)) {
                      {
                        tc_expr((node_a)[id]);
                        if ((((((((tc_kind != TY_BOOL) && (tc_kind != TY_INT)) && (tc_kind != TY_CHAR)) && (tc_kind != TY_FLOAT)) && (tc_kind != TY_DOUBLE)) && (tc_kind != TY_PTR)) && (tc_kind != TY_FUN))) {
                          tc_fail(26);
                        } else {
                          {
                          }
                        }
                        tc_loop_depth = (tc_loop_depth + 1);
                        tc_stmt((node_b)[id], expected_kind, expected_name);
                        tc_loop_depth = (tc_loop_depth - 1);
                      }
                    } else {
                      if ((k == N_FOR)) {
                        {
                          tc_enter_scope();
                          if (((node_a)[id] != 0)) {
                            tc_stmt((node_a)[id], expected_kind, expected_name);
                          } else {
                            {
                            }
                          }
                          tc_expr((node_b)[id]);
                          if ((((((((tc_kind != TY_BOOL) && (tc_kind != TY_INT)) && (tc_kind != TY_CHAR)) && (tc_kind != TY_FLOAT)) && (tc_kind != TY_DOUBLE)) && (tc_kind != TY_PTR)) && (tc_kind != TY_FUN))) {
                            tc_fail(27);
                          } else {
                            {
                            }
                          }
                          tc_loop_depth = (tc_loop_depth + 1);
                          tc_stmt((node_c)[id], expected_kind, expected_name);
                          tc_loop_depth = (tc_loop_depth - 1);
                          if (((node_value)[id] != 0)) {
                            tc_stmt((node_value)[id], expected_kind, expected_name);
                          } else {
                            {
                            }
                          }
                          tc_leave_scope();
                        }
                      } else {
                        {
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

int tc_diag_line(int pos) {
  int i = 0;
  int line = 1;
  while (((i < pos) && (i < source_len))) {
    {
      if (((source)[i] == 10)) {
        line = (line + 1);
      } else {
        {
        }
      }
      i = (i + 1);
    }
  }
  return line;
}

int tc_diag_col(int pos) {
  int i = 0;
  int col = 1;
  while (((i < pos) && (i < source_len))) {
    {
      if (((source)[i] == 10)) {
        col = 1;
      } else {
        col = (col + 1);
      }
      i = (i + 1);
    }
  }
  return col;
}

void tc_diag(void) {
  if ((tc_error_code == 3)) {
    printf("%s\n", "type error: duplicate declaration");
  } else {
    if ((tc_error_code == 5)) {
      printf("%s\n", "type error: unknown name");
    } else {
      if ((tc_error_code == 12)) {
        printf("%s\n", "type error: invalid function arguments");
      } else {
        if ((tc_error_code == 14)) {
          printf("%s\n", "type error: string concatenation requires strings");
        } else {
          if ((tc_error_code == 18)) {
            printf("%s\n", "type error: invalid arithmetic operands");
          } else {
            if ((tc_error_code == 20)) {
              printf("%s\n", "type error: initializer type mismatch");
            } else {
              if ((tc_error_code == 21)) {
                printf("%s\n", "type error: assignment type mismatch");
              } else {
                if ((tc_error_code == 23)) {
                  printf("%s\n", "type error: return type mismatch");
                } else {
                  if ((tc_error_code == 28)) {
                    printf("%s\n", "type error: recursive struct definition");
                  } else {
                    if ((tc_error_code == 31)) {
                      printf("%s\n", "type error: assignment to const");
                    } else {
                      if ((tc_error_code == 33)) {
                        printf("%s\n", "type error: use after ownership move");
                      } else {
                        if ((tc_error_code == 34)) {
                          printf("%s\n", "type error: use after ownership move");
                        } else {
                          if ((tc_error_code == 35)) {
                            printf("%s\n", "type error: release requires an owned value");
                          } else {
                            if ((tc_error_code == 36)) {
                              printf("%s\n", "type error: array element type mismatch");
                            } else {
                              if ((tc_error_code == 37)) {
                                printf("%s\n", "type error: cannot mutate or move while borrowed");
                              } else {
                                if ((tc_error_code == 38)) {
                                  printf("%s\n", "type error: borrowed reference escapes its owner");
                                } else {
                                  if ((tc_error_code == 40)) {
                                    printf("%s\n", "type error: owned value copy requires an explicit move");
                                  } else {
                                    printf("%s\n", "type error: invalid expression");
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
  printf("%d\n", tc_diag_line(tc_error_pos));
  printf("%d\n", tc_diag_col(tc_error_pos));
}

int tc_program(int root) {
  tc_root = root;
  tc_ok = 1;
  tc_error_code = 0;
  tc_var_count = 0;
  tc_scope_count = 0;
  tc_path_count = 0;
  tc_loop_depth = 0;
  tc_enter_scope();
  int item = (node_a)[root];
  while ((item != 0)) {
    {
      if (((node_kind)[item] == N_STRUCT)) {
        {
          int f = (node_a)[item];
          while ((f != 0)) {
            {
              tc_check_type((node_b)[f]);
              f = (node_next)[f];
            }
          }
          if ((tc_cycle_struct((node_value)[item]) == 1)) {
            tc_fail(28);
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  item = (node_a)[root];
  while ((item != 0)) {
    {
      if ((((node_kind)[item] == N_GLOBAL) || ((node_kind)[item] == N_CONST))) {
        {
          tc_type_node((node_b)[item]);
          int gk = tc_kind;
          int gn = tc_name;
          int ge = tc_elem_kind;
          int gen = tc_elem_name;
          tc_add_var((node_a)[item], gk, gn, ge, gen, (node_b)[item]);
          if (((node_kind)[item] == N_CONST)) {
            (sym_type)[(node_a)[item]] = (gk + 100);
          } else {
            {
            }
          }
          tc_expr((node_c)[item]);
          if ((tc_same_full(gk, gn, ge, gen, tc_kind, tc_name, tc_elem_kind, tc_elem_name) == 0)) {
            {
              int polymorphic_make = 0;
              if (((((node_kind)[(node_c)[item]] == N_CALL) && ((sym_len)[(node_value)[(node_c)[item]]] == 10)) && ((sym_hash)[(node_value)[(node_c)[item]]] == 790299))) {
                polymorphic_make = 1;
              } else {
                {
                }
              }
              if ((((polymorphic_make == 0) && (((node_kind)[(node_c)[item]] != N_INT) || ((node_value)[(node_c)[item]] != 0))) && ((node_kind)[(node_c)[item]] != N_NULL))) {
                tc_fail(29);
              } else {
                {
                }
              }
            }
          } else {
            {
            }
          }
        }
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  tc_global_count = tc_var_count;
  item = (node_a)[root];
  while ((item != 0)) {
    {
      if (((node_kind)[item] == N_FUNC)) {
        {
          tc_var_count = tc_global_count;
          tc_scope_count = 1;
          tc_enter_scope();
          int p = (node_c)[item];
          while ((p != 0)) {
            {
              tc_type_node((node_b)[p]);
              int pk = tc_kind;
              int pn = tc_name;
              int pek = tc_elem_kind;
              int pen = tc_elem_name;
              tc_add_var((node_a)[p], pk, pn, pek, pen, (node_b)[p]);
              if ((pk == TY_DYN_ARRAY)) {
                (tc_var_owned)[tc_last_var_index] = 1;
              } else {
                {
                }
              }
              p = (node_next)[p];
            }
          }
          tc_type_node((node_b)[item]);
          tc_expected_elem_kind = tc_elem_kind;
          tc_expected_elem_name = tc_elem_name;
          tc_stmt((node_a)[item], tc_kind, tc_name);
          tc_leave_scope();
        }
      } else {
        {
        }
      }
      item = (node_next)[item];
    }
  }
  if ((tc_ok == 0)) {
    {
      tc_diag();
      return 0;
    }
  } else {
    {
    }
  }
  return 1;
}

int pipeline_main(char* path) {
  load_tokens_from_file(path);
  node_count = 1;
  int root = ast_program();
  pipeline_root = root;
  int parsed = 1;
  if ((root < 0)) {
    parsed = 0;
  } else {
    {
    }
  }
  if ((input_peek() != T_EOF)) {
    parsed = 0;
  } else {
    {
    }
  }
  if ((include_ok == 0)) {
    parsed = 0;
  } else {
    {
    }
  }
  if ((parsed == 1)) {
    {
      if ((tc_program(root) == 0)) {
        parsed = 0;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((parsed == 1)) {
    {
      code_reset();
      gen_program(root);
      code_reset();
      gen_program(root);
      int stable_count = code_count;
      code_reset();
      gen_program(root);
      if ((code_count != stable_count)) {
        parsed = 0;
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((parsed == 0)) {
    {
      if ((tc_error_code == 0)) {
        {
          printf("%s\n", "parse error");
          printf("%d\n", tc_diag_line(source_pos));
          printf("%d\n", tc_diag_col(source_pos));
        }
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  if ((parsed == 1)) {
    {
      return 0;
    }
  } else {
    {
    }
  }
  return 1;
}

void emit_symbol(int* out, int id) {
  int i = 0;
  while ((i < (sym_len)[id])) {
    {
      write_char(out, (source)[((sym_start)[id] + i)]);
      i = (i + 1);
    }
  }
}

void emit_string(int* out, int id) {
  write_char(out, 34);
  emit_symbol(out, id);
  write_char(out, 34);
}

void emit_print_prefix(int* out) {
  write_string(out, "(");
  write_char(out, 34);
  write_string(out, "%d");
  write_char(out, 92);
  write_char(out, 110);
  write_char(out, 34);
  write_string(out, ", ");
}

void emit_int_text(int* out, int value) {
  if ((value < 0)) {
    {
      write_char(out, 45);
      emit_int_text(out, (0 - value));
    }
  } else {
    if ((value < 10)) {
      {
        write_char(out, (48 + value));
      }
    } else {
      {
        emit_int_text(out, (value / 10));
        write_char(out, (48 + (value - ((value / 10) * 10))));
      }
    }
  }
}

void emit_c_token(int* out, int kind, int value) {
  if ((kind == C_KW)) {
    {
      if ((value == 1)) {
        write_string(out, "int ");
      } else {
        if ((value == 2)) {
          write_string(out, "int ");
        } else {
          if ((value == 3)) {
            write_string(out, "char* ");
          } else {
            if ((value == 4)) {
              write_string(out, "void ");
            } else {
              if ((value == 5)) {
                write_string(out, "return ");
              } else {
                if ((value == 6)) {
                  write_string(out, "if ");
                } else {
                  if ((value == 7)) {
                    write_string(out, "else ");
                  } else {
                    if ((value == 8)) {
                      write_string(out, "while ");
                    } else {
                      if ((value == 9)) {
                        write_string(out, "break ");
                      } else {
                        if ((value == 10)) {
                          write_string(out, "continue ");
                        } else {
                          if ((value == 11)) {
                            write_string(out, "for ");
                          } else {
                            if ((value == 12)) {
                              write_string(out, "struct ");
                            } else {
                              if ((value == 13)) {
                                write_string(out, "enum ");
                              } else {
                                if ((value == 14)) {
                                  write_string(out, "typedef ");
                                } else {
                                  if ((value == 15)) {
                                    write_string(out, "double ");
                                  } else {
                                    if ((value == 16)) {
                                      write_string(out, "const ");
                                    } else {
                                      if ((value == 18)) {
                                        write_string(out, "float ");
                                      } else {
                                        if ((value == 17)) {
                                          write_string(out, "char ");
                                        } else {
                                          {
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  } else {
    if ((kind == C_IDENT)) {
      {
        if ((value == 1001)) {
          write_string(out, "printf");
        } else {
          if ((value == 1002)) {
            write_string(out, "runtime_string_concat");
          } else {
            if ((value == 1003)) {
              write_string(out, "AshArray");
            } else {
              if ((value == 1004)) {
                write_string(out, "runtime_array_make");
              } else {
                if ((value == 1005)) {
                  write_string(out, "runtime_array_push");
                } else {
                  if ((value == 1006)) {
                    write_string(out, "runtime_array_get");
                  } else {
                    if ((value == 1007)) {
                      write_string(out, "runtime_array_set");
                    } else {
                      if ((value == 1008)) {
                        write_string(out, "runtime_array_reserve");
                      } else {
                        if ((value == 1009)) {
                          write_string(out, "runtime_array_clear");
                        } else {
                          if ((value == 1010)) {
                            write_string(out, "runtime_array_free");
                          } else {
                            if ((value == 1011)) {
                              write_string(out, "sizeof");
                            } else {
                              if ((value == 1012)) {
                                write_string(out, "len");
                              } else {
                                if ((value == 1013)) {
                                  write_string(out, "cap");
                                } else {
                                  if ((value == 1014)) {
                                    write_string(out, "runtime_array_get_raw");
                                  } else {
                                    if ((value == 1015)) {
                                      write_string(out, "data");
                                    } else {
                                      emit_symbol(out, value);
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    } else {
      if ((kind == C_INT)) {
        emit_int_text(out, value);
      } else {
        if ((kind == C_STRING)) {
          emit_string(out, value);
        } else {
          if ((kind == C_OP)) {
            {
              if ((value == 1)) {
                write_string(out, "+");
              } else {
                if ((value == 2)) {
                  write_string(out, "-");
                } else {
                  if ((value == 3)) {
                    write_string(out, "*");
                  } else {
                    if ((value == 4)) {
                      write_string(out, "/");
                    } else {
                      if ((value == 5)) {
                        write_string(out, "==");
                      } else {
                        if ((value == 6)) {
                          write_string(out, "!=");
                        } else {
                          if ((value == 7)) {
                            write_string(out, "<");
                          } else {
                            if ((value == 8)) {
                              write_string(out, ">");
                            } else {
                              if ((value == 9)) {
                                write_string(out, "&&");
                              } else {
                                if ((value == 10)) {
                                  write_string(out, "||");
                                } else {
                                  if ((value == 11)) {
                                    write_string(out, "++");
                                  } else {
                                    if ((value == 12)) {
                                      write_string(out, "&");
                                    } else {
                                      if ((value == 13)) {
                                        write_string(out, "|");
                                      } else {
                                        if ((value == 14)) {
                                          write_string(out, "^");
                                        } else {
                                          if ((value == 15)) {
                                            write_string(out, "<<");
                                          } else {
                                            if ((value == 16)) {
                                              write_string(out, ">>");
                                            } else {
                                              if ((value == 17)) {
                                                write_string(out, "%");
                                              } else {
                                                if ((value == 18)) {
                                                  write_string(out, ":");
                                                } else {
                                                  {
                                                  }
                                                }
                                              }
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            }
          } else {
            if ((kind == C_PUNCT)) {
              {
                if ((value == 1)) {
                  write_string(out, "*");
                } else {
                  if ((value == 2)) {
                    write_string(out, "[");
                  } else {
                    if ((value == 3)) {
                      write_string(out, "]");
                    } else {
                      if ((value == 4)) {
                        write_string(out, "(");
                      } else {
                        if ((value == 5)) {
                          write_string(out, ")");
                        } else {
                          if ((value == 6)) {
                            write_string(out, "(");
                          } else {
                            if ((value == 7)) {
                              write_string(out, ", ");
                            } else {
                              if ((value == 8)) {
                                write_string(out, ")");
                              } else {
                                if ((value == 9)) {
                                  write_string(out, "*");
                                } else {
                                  if ((value == 10)) {
                                    write_string(out, "&");
                                  } else {
                                    if ((value == 11)) {
                                      write_string(out, " = ");
                                    } else {
                                      if ((value == 12)) {
                                        write_string(out, ";");
                                      } else {
                                        if ((value == 13)) {
                                          write_string(out, "{\n");
                                        } else {
                                          if ((value == 14)) {
                                            write_string(out, "}\n");
                                          } else {
                                            if ((value == 15)) {
                                              emit_print_prefix(out);
                                            } else {
                                              if ((value == 16)) {
                                                {
                                                  write_char(out, 40);
                                                  write_char(out, 34);
                                                  write_string(out, "%s");
                                                  write_char(out, 92);
                                                  write_char(out, 110);
                                                  write_char(out, 34);
                                                  write_string(out, ", ");
                                                }
                                              } else {
                                                if ((value == 17)) {
                                                  write_string(out, ".");
                                                } else {
                                                  if ((value == 20)) {
                                                    {
                                                      write_char(out, 40);
                                                      write_char(out, 34);
                                                      write_string(out, "%c");
                                                      write_char(out, 92);
                                                      write_char(out, 110);
                                                      write_char(out, 34);
                                                      write_string(out, ", ");
                                                    }
                                                  } else {
                                                    if ((value == 21)) {
                                                      {
                                                        write_char(out, 40);
                                                        write_char(out, 34);
                                                        write_string(out, "%g");
                                                        write_char(out, 92);
                                                        write_char(out, 110);
                                                        write_char(out, 34);
                                                        write_string(out, ", ");
                                                      }
                                                    } else {
                                                      if ((value == 18)) {
                                                        write_string(out, " ");
                                                      } else {
                                                        if ((value == 19)) {
                                                          write_string(out, "{0}");
                                                        } else {
                                                          if ((value == 22)) {
                                                            write_char(out, 32);
                                                          } else {
                                                            if ((value == 23)) {
                                                              {
                                                                write_char(out, 40);
                                                                write_char(out, 34);
                                                                write_string(out, "%td");
                                                                write_char(out, 92);
                                                                write_char(out, 110);
                                                                write_char(out, 34);
                                                                write_string(out, ", ");
                                                              }
                                                            } else {
                                                              if ((value == 24)) {
                                                                write_string(out, "{");
                                                              } else {
                                                                if ((value == 25)) {
                                                                  write_string(out, "}");
                                                                } else {
                                                                  {
                                                                  }
                                                                }
                                                              }
                                                            }
                                                          }
                                                        }
                                                      }
                                                    }
                                                  }
                                                }
                                              }
                                            }
                                          }
                                        }
                                      }
                                    }
                                  }
                                }
                              }
                            }
                          }
                        }
                      }
                    }
                  }
                }
              }
            } else {
              if ((kind == C_NEWLINE)) {
                write_char(out, 10);
              } else {
                {
                }
              }
            }
          }
        }
      }
    }
  }
}

void emit_runtime(int* out) {
  write_string(out, "#if defined(_WIN32)\n#include <direct.h>\n#else\n#define _POSIX_C_SOURCE 200809L\n#define _XOPEN_SOURCE 700\n#endif\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <limits.h>\n#if defined(__GNUC__) || defined(__clang__)\n#define ASH_UNUSED __attribute__((unused))\n#else\n#define ASH_UNUSED\n#endif\nstatic void ash_panic(int code){(void)code;exit(2);}\nstatic size_t ash_checked_bytes(int count,size_t elem_size){if(count<0)ash_panic(1);if(elem_size!=0&&(size_t)count>(size_t)-1/elem_size)ash_panic(1);return(size_t)count*elem_size;}\nstatic void* ash_track(void*);\n");
  write_string(out, "static char** ash_inc_active=NULL;static size_t ash_inc_active_n=0,ash_inc_active_cap=0;static char** ash_inc_loaded=NULL;static size_t ash_inc_loaded_n=0,ash_inc_loaded_cap=0;static int ash_inc_status=0;\n");
  write_string(out, "static ASH_UNUSED int ash_inc_eq(const char*a,const char*b){return strcmp(a,b)==0;}\n");
  write_string(out, "static ASH_UNUSED size_t ash_inc_find(char**v,size_t n,const char*p){size_t i;for(i=0;i<n;i++)if(ash_inc_eq(v[i],p))return i;return (size_t)-1;}\n");
  write_string(out, "static ASH_UNUSED void ash_inc_add(char***vp,size_t*np,size_t*cp,char*p){size_t c;char**q;if(*np==*cp){c=*cp?*cp*2:16;q=(char**)realloc(*vp,c*sizeof(char*));if(!q)exit(2);*vp=q;*cp=c;}(*vp)[(*np)++]=p;}\n");
  write_string(out, "static ASH_UNUSED char* ash_inc_strdup(const char*p){size_t n=strlen(p);char*q=(char*)malloc(n+1);if(!q)exit(2);memcpy(q,p,n+1);return(char*)ash_track(q);}\n");
  write_string(out, "static ASH_UNUSED char* ash_inc_realpath(const char*p){\n#if defined(_WIN32)\nchar*q=_fullpath(NULL,p,0);if(q)return(char*)ash_track(q);\n#else\nchar*q=realpath(p,NULL);if(q)return(char*)ash_track(q);\n#endif\nreturn ash_inc_strdup(p);}\n");
  write_string(out, "static ASH_UNUSED int ash_inc_begin(char*p){if(ash_inc_find(ash_inc_active,ash_inc_active_n,p)!=(size_t)-1){ash_inc_status=1;return 0;}if(ash_inc_find(ash_inc_loaded,ash_inc_loaded_n,p)!=(size_t)-1){ash_inc_status=2;return 0;}ash_inc_add(&ash_inc_active,&ash_inc_active_n,&ash_inc_active_cap,p);ash_inc_status=0;return 1;}\n");
  write_string(out, "static ASH_UNUSED void ash_include_close(void){if(ash_inc_active_n){char*p=ash_inc_active[--ash_inc_active_n];if(ash_inc_find(ash_inc_loaded,ash_inc_loaded_n,p)==(size_t)-1)ash_inc_add(&ash_inc_loaded,&ash_inc_loaded_n,&ash_inc_loaded_cap,p);}}\n");
  write_string(out, "static ASH_UNUSED char* ash_inc_join(const char*base,const char*raw){const char*s=strrchr(base,'/');size_t n=s?(size_t)(s-base+1):0;size_t m=strlen(raw);char*q=(char*)malloc(n+m+1);if(!q)exit(2);if(n)memcpy(q,base,n);memcpy(q+n,raw,m+1);return(char*)ash_track(q);}\n");
  write_string(out, "static ASH_UNUSED int ash_include_line_mode(int*line,int n){int i=0,j;while(i<n&&(line[i]==' '||line[i]==9))i++;if(i+7<=n&&line[i]=='i'&&line[i+1]=='n'&&line[i+2]=='c'&&line[i+3]=='l'&&line[i+4]=='u'&&line[i+5]=='d'&&line[i+6]=='e'){j=i+7;while(j<n&&(line[j]==' '||line[j]==9))j++;if(j<n&&line[j]==34)return 1;}if(i+8<=n&&line[i]=='i'&&line[i+1]=='n'&&line[i+2]=='c'&&line[i+3]=='l'&&line[i+4]=='u'&&line[i+5]=='d'&&line[i+6]=='e'&&line[i+7]=='c'){j=i+8;while(j<n&&(line[j]==' '||line[j]==9))j++;if(j<n&&line[j]==34)return 2;}return 0;}\n");
  write_string(out, "static ASH_UNUSED void* ash_include_open_root(const char*path){char*p=ash_inc_realpath(path);FILE*f;if(!ash_inc_begin(p))return NULL;f=fopen(p,(const char[]){114,0});if(!f){ash_inc_status=3;ash_include_close();return NULL;}return(void*)f;}\n");
  write_string(out, "static ASH_UNUSED void* ash_include_open_line(int*line,int n,int mode){int i=0,a,b,j;char*raw,*joined,*canon;FILE*f;(void)mode;while(i<n&&line[i]!=34)i++;if(i>=n)return NULL;a=++i;while(i<n&&line[i]!=34)i++;if(i>=n)return NULL;b=i;raw=(char*)malloc((size_t)(b-a)+1);if(!raw)exit(2);{int k;for(k=0;k<b-a;k++)raw[k]=(char)line[a+k];}raw[b-a]=0;raw=(char*)ash_track(raw);j=i+1;while(j<n&&(line[j]==32||line[j]==9))j++;if(j<n&&line[j]==59)j++;while(j<n&&(line[j]==32||line[j]==9))j++;if(j!=n)return NULL;joined=ash_inc_join(ash_inc_active[ash_inc_active_n-1],raw);canon=ash_inc_realpath(joined);if(!ash_inc_begin(canon))return NULL;f=fopen(canon,(const char[]){114,0});if(!f){ash_inc_status=3;ash_include_close();return NULL;}return(void*)f;}\n");
  write_string(out, "static ASH_UNUSED int ash_include_last_status(void){return ash_inc_status;}\n");
  write_string(out, "static ASH_UNUSED void ash_include_reset_session(void){ash_inc_active_n=0;ash_inc_loaded_n=0;ash_inc_status=0;}\n");
  write_string(out, "static ASH_UNUSED void* open_file(const char* p,const char* m){return (void*)fopen(p,m);}\n");
  write_string(out, "static ASH_UNUSED int read_char(void* h){int c=fgetc((FILE*)h);return c==EOF?-1:c;}\n");
  write_string(out, "static ASH_UNUSED int close_file(void* h){return fclose((FILE*)h);}\n");
  write_string(out, "static ASH_UNUSED int write_char(void* h,int c){return fputc(c,(FILE*)h);}\n");
  write_string(out, "static ASH_UNUSED int write_string(void* h,const char* s){return fputs(s,(FILE*)h);}\n");
  write_string(out, "static void** ash_live=NULL;static size_t ash_live_n=0,ash_live_cap=0;\n");
  write_string(out, "static ASH_UNUSED size_t ash_find(void* p){size_t i;for(i=0;i<ash_live_n;i++)if(ash_live[i]==p)return i;return (size_t)-1;}\n");
  write_string(out, "static ASH_UNUSED void ash_validate(void){size_t i,j;for(i=0;i<ash_live_n;i++){if(!ash_live[i])ash_panic(2);for(j=i+1;j<ash_live_n;j++)if(ash_live[i]==ash_live[j])ash_panic(2);}}\n");
  write_string(out, "static ASH_UNUSED void ash_cleanup(void){size_t i;ash_validate();for(i=0;i<ash_live_n;i++)free(ash_live[i]);free(ash_live);ash_live=NULL;ash_live_n=ash_live_cap=0;}\n");
  write_string(out, "static ASH_UNUSED void* ash_track(void* p){size_t c;void**q;if(!p)return NULL;if(ash_find(p)!=(size_t)-1)ash_panic(2);if(ash_live_n==ash_live_cap){if(ash_live_cap>(size_t)-1/2)c=(size_t)-1;else c=ash_live_cap?ash_live_cap*2:32;if(c>(size_t)-1/sizeof(void*))ash_panic(2);q=(void**)realloc(ash_live,c*sizeof(void*));if(!q)ash_panic(2);ash_live=q;ash_live_cap=c;}ash_live[ash_live_n++]=p;atexit(ash_cleanup);return p;}\n");
  write_string(out, "static ASH_UNUSED void ash_release(void* p){size_t i;if(!p)return;i=ash_find(p);if(i==(size_t)-1)ash_panic(2);free(p);ash_live[i]=ash_live[--ash_live_n];}\n");
  write_string(out, "static ASH_UNUSED char* runtime_string_concat(const char* a,const char* b){size_t na,nb,total;char* p;if(!a||!b)ash_panic(4);na=strlen(a);nb=strlen(b);if(na>(size_t)-1-nb-1)ash_panic(1);total=na+nb+1;p=(char*)malloc(total);if(!p)ash_panic(5);memcpy(p,a,na);memcpy(p+na,b,nb);p[na+nb]=0;return(char*)ash_track(p);}\n");
  write_string(out, "typedef struct AshArray { void* data; int len; int cap; } AshArray;\n");
  write_string(out, "static ASH_UNUSED void ash_array_check(const AshArray* a){if(!a)ash_panic(4);if(a->len<0||a->cap<0||a->len>a->cap)ash_panic(3);if(a->cap>0&&(!a->data||ash_find(a->data)==(size_t)-1))ash_panic(2);}\n");
  write_string(out, "static ASH_UNUSED AshArray runtime_array_make(int capacity,size_t elem_size){AshArray a;if(capacity<0)ash_panic(1);if(capacity<1)capacity=4;ash_checked_bytes(capacity,elem_size);a.data=calloc((size_t)capacity,elem_size);if(!a.data)ash_panic(5);a.len=0;a.cap=capacity;a.data=ash_track(a.data);return a;}\n");
  write_string(out, "static ASH_UNUSED AshArray runtime_array_reserve(AshArray* a,int minimum,size_t elem_size){void*p;ash_array_check(a);if(minimum<0)ash_panic(1);if(minimum<=a->cap)return *a;ash_checked_bytes(minimum,elem_size);p=calloc((size_t)minimum,elem_size);if(!p)ash_panic(5);if(a->data){memcpy(p,a->data,ash_checked_bytes(a->len,elem_size));ash_release(a->data);}a->data=ash_track(p);a->cap=minimum;return *a;}\n");
  write_string(out, "static ASH_UNUSED AshArray runtime_array_push(AshArray* a,const void* value,size_t elem_size){int next;ash_array_check(a);if(!value)ash_panic(4);if(a->len>=a->cap){if(a->cap>2147483647/2)ash_panic(1);next=a->cap>0?a->cap*2:4;runtime_array_reserve(a,next,elem_size);}memcpy((char*)a->data+(size_t)a->len*elem_size,value,elem_size);a->len+=1;return *a;}\n");
  write_string(out, "static ASH_UNUSED void runtime_array_set(AshArray* a,int index,const void* value,size_t elem_size){ash_array_check(a);if(index<0||index>=a->len)ash_panic(3);if(!value)ash_panic(4);memcpy((char*)a->data+ash_checked_bytes(index,elem_size),value,elem_size);}\n");
  write_string(out, "static ASH_UNUSED AshArray runtime_array_clear(AshArray* a){ash_array_check(a);a->len=0;return *a;}\n");
  write_string(out, "static ASH_UNUSED void runtime_array_free(AshArray* a){if(!a)ash_panic(4);if(a->data){if(ash_find(a->data)==(size_t)-1)ash_panic(2);ash_release(a->data);}a->data=NULL;a->len=0;a->cap=0;}\n");
  write_string(out, "static ASH_UNUSED void* runtime_array_get_raw(AshArray* a,int index,size_t elem_size){ash_array_check(a);if(index<0||index>=a->len)ash_panic(3);return(char*)a->data+ash_checked_bytes(index,elem_size);}\n");
  write_string(out, "static ASH_UNUSED int* alloc_ints(int n){int* p;if(n<0)ash_panic(1);if(n<1)n=1;ash_checked_bytes(n,sizeof(int));p=(int*)calloc((size_t)n,sizeof(int));if(!p)ash_panic(5);return(int*)ash_track(p);}\n");
  write_string(out, "static ASH_UNUSED void free_ints(int* p){ash_release(p);}\n");
  write_string(out, "static ASH_UNUSED int* grow_ints(int* p,int old,int n){size_t slot=(size_t)-1;int* q;if(old<0||n<0)ash_panic(1);if(n<=old)return p;if(p){slot=ash_find(p);if(slot==(size_t)-1)ash_panic(2);}ash_checked_bytes(n,sizeof(int));q=(int*)realloc(p,(size_t)n*sizeof(int));if(!q)ash_panic(6);if(p)ash_live[slot]=q;else ash_track(q);memset(q+old,0,(size_t)(n-old)*sizeof(int));return q;}\n");
  write_string(out, "extern int* payload_int; extern int* payload_name; extern int* payload_string;\n");
  write_string(out, "extern int* code_kind; extern int* code_value; extern int* input_kind; extern int* input_value;\n");
  write_string(out, "extern int* source; extern int* sym_start; extern int* sym_len; extern int* sym_hash; extern int* sym_kind; extern int* sym_type; extern int* sym_scope;\n\n");
}

void emit_c_file(char* path) {
  int* out = (void*)open_file(path, "w");
  emit_runtime(out);
  int ci = 0;
  while ((ci < c_source_len)) {
    {
      write_char(out, (c_source)[ci]);
      ci = (ci + 1);
    }
  }
  int i = 0;
  while ((i < code_count)) {
    {
      emit_c_token(out, (code_kind)[i], (code_value)[i]);
      i = (i + 1);
    }
  }
  (int)close_file(out);
}

int main(int argc, char** argv) {
{
  if ((argc < 2)) {
    {
      return 1;
    }
  } else {
    {
    }
  }
  int ok = pipeline_main((argv)[1]);
  if (((argc > 2) && (ok == 0))) {
    {
      if ((pipeline_root > 0)) {
        {
          gen_program(pipeline_root);
          emit_c_file((argv)[2]);
        }
      } else {
        {
        }
      }
    }
  } else {
    {
    }
  }
  return ok;
}
  return 0;
}
