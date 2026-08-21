#if defined(_WIN32)
#include <direct.h>
#else
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#include <stdatomic.h>
#include <threads.h>
#if defined(__GNUC__) || defined(__clang__)
#define BASALT_UNUSED __attribute__((unused))
#else
#define BASALT_UNUSED
#endif
static void basalt_panic(int code){(void)code;exit(2);}
static size_t basalt_checked_bytes(int count,size_t elem_size){if(count<0)basalt_panic(1);if(elem_size!=0&&(size_t)count>(size_t)-1/elem_size)basalt_panic(1);return(size_t)count*elem_size;}
static void* basalt_track(void*);static void basalt_release(void*);
typedef struct basalt_atomic_int { atomic_int value; } basalt_atomic_int;
static BASALT_UNUSED void* basalt_atomic_make(int initial){basalt_atomic_int*a=(basalt_atomic_int*)calloc(1,sizeof(*a));if(!a)basalt_panic(5);atomic_init(&a->value,initial);return basalt_track(a);}
static BASALT_UNUSED int basalt_atomic_load(void*p){basalt_atomic_int*a=(basalt_atomic_int*)p;if(!a)basalt_panic(4);return atomic_load_explicit(&a->value,memory_order_acquire);}
static BASALT_UNUSED void basalt_atomic_store(void*p,int value){basalt_atomic_int*a=(basalt_atomic_int*)p;if(!a)basalt_panic(4);atomic_store_explicit(&a->value,value,memory_order_release);}
static BASALT_UNUSED int basalt_atomic_fetch_add(void*p,int delta){basalt_atomic_int*a=(basalt_atomic_int*)p;if(!a)basalt_panic(4);return atomic_fetch_add_explicit(&a->value,delta,memory_order_acq_rel);}
static BASALT_UNUSED int basalt_atomic_compare_exchange(void*p,int expected,int desired){basalt_atomic_int*a=(basalt_atomic_int*)p;int old;if(!a)basalt_panic(4);old=expected;return atomic_compare_exchange_strong_explicit(&a->value,&old,desired,memory_order_acq_rel,memory_order_acquire);}
static BASALT_UNUSED void basalt_atomic_free(void*p){basalt_release(p);}
typedef struct basalt_channel { _Atomic size_t head; _Atomic size_t tail; atomic_int closed; size_t capacity; int data[]; } basalt_channel;
static BASALT_UNUSED void* basalt_channel_make(int requested){size_t cap=2;size_t bytes;basalt_channel*c;if(requested<1||requested>1073741824)basalt_panic(7);while(cap<(size_t)requested){if(cap>(size_t)-1/2)basalt_panic(1);cap*=2;}if(cap>(size_t)-1/sizeof(int))basalt_panic(1);bytes=sizeof(*c)+cap*sizeof(int);if(bytes<sizeof(*c))basalt_panic(1);c=(basalt_channel*)calloc(1,bytes);if(!c)basalt_panic(5);c->capacity=cap;atomic_init(&c->head,0);atomic_init(&c->tail,0);atomic_init(&c->closed,0);return basalt_track(c);}
static BASALT_UNUSED int basalt_channel_send(void*p,int value){basalt_channel*c=(basalt_channel*)p;size_t head,tail;if(!c)basalt_panic(4);if(atomic_load_explicit(&c->closed,memory_order_acquire)!=0)return -1;head=atomic_load_explicit(&c->head,memory_order_relaxed);tail=atomic_load_explicit(&c->tail,memory_order_acquire);if(head-tail>=c->capacity)return 0;c->data[head&(c->capacity-1)]=value;atomic_store_explicit(&c->head,head+1,memory_order_release);return 1;}
static BASALT_UNUSED int basalt_channel_recv(void*p,int*out){basalt_channel*c=(basalt_channel*)p;size_t head,tail;if(!c||!out)basalt_panic(4);tail=atomic_load_explicit(&c->tail,memory_order_relaxed);head=atomic_load_explicit(&c->head,memory_order_acquire);if(tail==head){if(atomic_load_explicit(&c->closed,memory_order_acquire)!=0)return -1;return 0;}*out=c->data[tail&(c->capacity-1)];atomic_store_explicit(&c->tail,tail+1,memory_order_release);return 1;}
static BASALT_UNUSED void basalt_channel_close(void*p){basalt_channel*c=(basalt_channel*)p;if(!c)basalt_panic(4);atomic_store_explicit(&c->closed,1,memory_order_release);}
static BASALT_UNUSED void basalt_channel_free(void*p){basalt_release(p);}
typedef struct basalt_thread_handle { thrd_t thread; } basalt_thread_handle;
static BASALT_UNUSED void* basalt_thread_spawn(int(*entry)(void*),void*arg){basalt_thread_handle*h=(basalt_thread_handle*)calloc(1,sizeof(*h));if(!h)basalt_panic(5);if(thrd_create(&h->thread,entry,arg)!=thrd_success){free(h);return NULL;}return basalt_track(h);}
static BASALT_UNUSED int basalt_thread_join(void*p){basalt_thread_handle*h=(basalt_thread_handle*)p;int result;if(!h)basalt_panic(4);if(thrd_join(h->thread,&result)!=thrd_success)basalt_panic(8);basalt_release(h);return result;}
static BASALT_UNUSED void basalt_thread_yield(void){thrd_yield();}
static char** basalt_inc_active=NULL;static size_t basalt_inc_active_n=0,basalt_inc_active_cap=0;static char** basalt_inc_loaded=NULL;static size_t basalt_inc_loaded_n=0,basalt_inc_loaded_cap=0;static int basalt_inc_status=0;
static BASALT_UNUSED int basalt_inc_eq(const char*a,const char*b){return strcmp(a,b)==0;}
static BASALT_UNUSED size_t basalt_inc_find(char**v,size_t n,const char*p){size_t i;for(i=0;i<n;i++)if(basalt_inc_eq(v[i],p))return i;return (size_t)-1;}
static BASALT_UNUSED void basalt_inc_add(char***vp,size_t*np,size_t*cp,char*p){size_t c;char**q;if(*np==*cp){c=*cp?*cp*2:16;q=(char**)realloc(*vp,c*sizeof(char*));if(!q)exit(2);*vp=q;*cp=c;}(*vp)[(*np)++]=p;}
static BASALT_UNUSED char* basalt_inc_strdup(const char*p){size_t n=strlen(p);char*q=(char*)malloc(n+1);if(!q)exit(2);memcpy(q,p,n+1);return(char*)basalt_track(q);}
static BASALT_UNUSED char* basalt_inc_realpath(const char*p){if(p&&p[0]==0&&basalt_inc_active_n)return basalt_inc_active[basalt_inc_active_n-1];
#if defined(_WIN32)
char*q=_fullpath(NULL,p,0);if(q)return(char*)basalt_track(q);
#else
char*q=realpath(p,NULL);if(q)return(char*)basalt_track(q);
#endif
return basalt_inc_strdup(p);}
static BASALT_UNUSED int basalt_inc_begin(char*p){if(basalt_inc_find(basalt_inc_active,basalt_inc_active_n,p)!=(size_t)-1){basalt_inc_status=1;return 0;}if(basalt_inc_find(basalt_inc_loaded,basalt_inc_loaded_n,p)!=(size_t)-1){basalt_inc_status=2;return 0;}basalt_inc_add(&basalt_inc_active,&basalt_inc_active_n,&basalt_inc_active_cap,p);basalt_inc_status=0;return 1;}
static BASALT_UNUSED void basalt_include_close(void){if(basalt_inc_active_n){char*p=basalt_inc_active[--basalt_inc_active_n];if(basalt_inc_find(basalt_inc_loaded,basalt_inc_loaded_n,p)==(size_t)-1)basalt_inc_add(&basalt_inc_loaded,&basalt_inc_loaded_n,&basalt_inc_loaded_cap,p);}}
static BASALT_UNUSED char* basalt_inc_join(const char*base,const char*raw){const char*s=strrchr(base,'/');size_t n=s?(size_t)(s-base+1):0;size_t m=strlen(raw);char*q=(char*)malloc(n+m+1);if(!q)exit(2);if(n)memcpy(q,base,n);memcpy(q+n,raw,m+1);return(char*)basalt_track(q);}
static BASALT_UNUSED int basalt_include_line_mode(int*line,int n){int i=0,j;while(i<n&&(line[i]==' '||line[i]==9))i++;if(i+7<=n&&line[i]=='i'&&line[i+1]=='n'&&line[i+2]=='c'&&line[i+3]=='l'&&line[i+4]=='u'&&line[i+5]=='d'&&line[i+6]=='e'){j=i+7;while(j<n&&(line[j]==' '||line[j]==9))j++;if(j<n&&line[j]==34)return 1;}if(i+8<=n&&line[i]=='i'&&line[i+1]=='n'&&line[i+2]=='c'&&line[i+3]=='l'&&line[i+4]=='u'&&line[i+5]=='d'&&line[i+6]=='e'&&line[i+7]=='c'){j=i+8;while(j<n&&(line[j]==' '||line[j]==9))j++;if(j<n&&line[j]==34)return 2;}return 0;}
static BASALT_UNUSED void* basalt_include_open_root(const char*path){char*p=basalt_inc_realpath(path);FILE*f;if(!basalt_inc_begin(p))return NULL;f=fopen(p,(const char[]){114,0});if(!f){basalt_inc_status=3;basalt_include_close();return NULL;}return(void*)f;}
static BASALT_UNUSED void* basalt_include_open_line(int*line,int n,int mode){int i=0,a,b,j;char*raw,*joined,*canon;FILE*f;(void)mode;while(i<n&&line[i]!=34)i++;if(i>=n)return NULL;a=++i;while(i<n&&line[i]!=34)i++;if(i>=n)return NULL;b=i;raw=(char*)malloc((size_t)(b-a)+1);if(!raw)exit(2);{int k;for(k=0;k<b-a;k++)raw[k]=(char)line[a+k];}raw[b-a]=0;raw=(char*)basalt_track(raw);j=i+1;while(j<n&&(line[j]==32||line[j]==9))j++;if(j<n&&line[j]==59)j++;while(j<n&&(line[j]==32||line[j]==9))j++;if(j!=n)return NULL;joined=basalt_inc_join(basalt_inc_active[basalt_inc_active_n-1],raw);canon=basalt_inc_realpath(joined);if(!basalt_inc_begin(canon))return NULL;f=fopen(canon,(const char[]){114,0});if(!f){basalt_inc_status=3;basalt_include_close();return NULL;}return(void*)f;}
static BASALT_UNUSED int basalt_include_last_status(void){return basalt_inc_status;}
static BASALT_UNUSED void basalt_include_reset_session(void){basalt_inc_active_n=0;basalt_inc_loaded_n=0;basalt_inc_status=0;}
static BASALT_UNUSED void* open_file(const char* p,const char* m){return (void*)fopen(p,m);}
static BASALT_UNUSED int read_char(void* h){int c=fgetc((FILE*)h);return c==EOF?-1:c;}
static BASALT_UNUSED int close_file(void* h){return fclose((FILE*)h);}
static BASALT_UNUSED int write_char(void* h,int c){return fputc(c,(FILE*)h);}
static BASALT_UNUSED int write_string(void* h,const char* s){return fputs(s,(FILE*)h);}
static void** basalt_live=NULL;static size_t basalt_live_n=0,basalt_live_cap=0;
static BASALT_UNUSED size_t basalt_find(void* p){size_t i;for(i=0;i<basalt_live_n;i++)if(basalt_live[i]==p)return i;return (size_t)-1;}
static BASALT_UNUSED void basalt_validate(void){size_t i,j;for(i=0;i<basalt_live_n;i++){if(!basalt_live[i])basalt_panic(2);for(j=i+1;j<basalt_live_n;j++)if(basalt_live[i]==basalt_live[j])basalt_panic(2);}}
static BASALT_UNUSED void basalt_cleanup(void){size_t i;basalt_validate();for(i=0;i<basalt_live_n;i++)free(basalt_live[i]);free(basalt_live);basalt_live=NULL;basalt_live_n=basalt_live_cap=0;}
static BASALT_UNUSED void* basalt_track(void* p){size_t c;void**q;if(!p)return NULL;if(basalt_find(p)!=(size_t)-1)basalt_panic(2);if(basalt_live_n==basalt_live_cap){if(basalt_live_cap>(size_t)-1/2)c=(size_t)-1;else c=basalt_live_cap?basalt_live_cap*2:32;if(c>(size_t)-1/sizeof(void*))basalt_panic(2);q=(void**)realloc(basalt_live,c*sizeof(void*));if(!q)basalt_panic(2);basalt_live=q;basalt_live_cap=c;}basalt_live[basalt_live_n++]=p;atexit(basalt_cleanup);return p;}
static BASALT_UNUSED void basalt_release(void* p){size_t i;if(!p)return;i=basalt_find(p);if(i==(size_t)-1)basalt_panic(2);free(p);basalt_live[i]=basalt_live[--basalt_live_n];}
static int basalt_io_status=0;
static BASALT_UNUSED int runtime_io_status(void){return basalt_io_status;}
static BASALT_UNUSED char* runtime_read_line(int max_len){size_t n=0;int c=EOF;char*p;if(max_len<2||max_len>1048576)basalt_panic(7);p=(char*)malloc((size_t)max_len);if(!p)basalt_panic(5);while(n+1<(size_t)max_len){c=fgetc(stdin);if(c==EOF)break;if(c=='\n')break;p[n++]=(char)c;}p[n]=0;if(c!=EOF&&c!='\n'&&n+1==(size_t)max_len){basalt_io_status=3;do{c=fgetc(stdin);}while(c!=EOF&&c!='\n');}else if(c==EOF&&n==0)basalt_io_status=1;else basalt_io_status=0;return(char*)basalt_track(p);}
static BASALT_UNUSED int runtime_read_int(int fallback){char buf[128];size_t n=0;int c=EOF;char*end;long v;while(n+1<sizeof(buf)){c=fgetc(stdin);if(c==EOF||c=='\n')break;if(c!='\r')buf[n++]=(char)c;}buf[n]=0;if(c!=EOF&&c!='\n'&&n+1==sizeof(buf)){basalt_io_status=3;do{c=fgetc(stdin);}while(c!=EOF&&c!='\n');return fallback;}if(c==EOF&&n==0){basalt_io_status=1;return fallback;}errno=0;v=strtol(buf,&end,10);while(*end==' '||*end=='\t'||*end=='\r')end++;if(end==buf||*end!=0){basalt_io_status=2;return fallback;}if(errno==ERANGE||v<(long)INT_MIN||v>(long)INT_MAX){basalt_io_status=4;return fallback;}basalt_io_status=0;return(int)v;}
static BASALT_UNUSED void runtime_write_string(const char*s){if(!s)basalt_panic(4);if(fputs(s,stdout)==EOF||fflush(stdout)!=0)basalt_panic(8);}
static BASALT_UNUSED void runtime_write_line(const char*s){if(!s)basalt_panic(4);if(fputs(s,stdout)==EOF||fputc('\n',stdout)==EOF||fflush(stdout)!=0)basalt_panic(8);}
static BASALT_UNUSED void runtime_write_int(int value){if(fprintf(stdout,"%d",value)<0||fflush(stdout)!=0)basalt_panic(8);}
static BASALT_UNUSED void runtime_write_char(char value){if(fputc((unsigned char)value,stdout)==EOF||fflush(stdout)!=0)basalt_panic(8);}
static BASALT_UNUSED void* basalt_memory_alloc(int count,size_t elem_size){size_t bytes=basalt_checked_bytes(count,elem_size);void*p=calloc(1,bytes?bytes:1);if(!p)basalt_panic(5);return basalt_track(p);}
static BASALT_UNUSED void* basalt_memory_alloc_aligned(int count,int alignment,size_t elem_size){size_t bytes,rounded,a;void*p;if(count<0||alignment<1)basalt_panic(1);a=(size_t)alignment;if((a&(a-1))!=0)basalt_panic(1);if(a<sizeof(void*))a=sizeof(void*);bytes=basalt_checked_bytes(count,elem_size);if(bytes==0)bytes=1;if(bytes>(size_t)-1-(a-1))basalt_panic(1);rounded=(bytes+a-1)&~(a-1);p=aligned_alloc(a,rounded);if(!p)basalt_panic(5);return basalt_track(p);}
static BASALT_UNUSED void* basalt_memory_resize(void* old,int old_count,int new_count,size_t elem_size){size_t slot=(size_t)-1;size_t old_bytes;size_t new_bytes;void*p;if(old_count<0||new_count<0||new_count<old_count)basalt_panic(1);if(old){slot=basalt_find(old);if(slot==(size_t)-1)basalt_panic(2);}old_bytes=basalt_checked_bytes(old_count,elem_size);new_bytes=basalt_checked_bytes(new_count,elem_size);p=realloc(old,new_bytes?new_bytes:1);if(!p)basalt_panic(6);if(slot==(size_t)-1)basalt_track(p);else basalt_live[slot]=p;if(new_bytes>old_bytes)memset((char*)p+old_bytes,0,new_bytes-old_bytes);return p;}
static BASALT_UNUSED void basalt_memory_free(void*p){basalt_release(p);}
static BASALT_UNUSED char* runtime_string_concat(const char* a,const char* b){size_t na,nb,total;char* p;if(!a||!b)basalt_panic(4);na=strlen(a);nb=strlen(b);if(na>(size_t)-1-nb-1)basalt_panic(1);total=na+nb+1;p=(char*)malloc(total);if(!p)basalt_panic(5);memcpy(p,a,na);memcpy(p+na,b,nb);p[na+nb]=0;return(char*)basalt_track(p);}
static BASALT_UNUSED int* alloc_ints(int n){int* p;if(n<0)basalt_panic(1);if(n<1)n=1;basalt_checked_bytes(n,sizeof(int));p=(int*)calloc((size_t)n,sizeof(int));if(!p)basalt_panic(5);return(int*)basalt_track(p);}
static BASALT_UNUSED void free_ints(int* p){basalt_release(p);}
static BASALT_UNUSED int* grow_ints(int* p,int old,int n){size_t slot=(size_t)-1;int* q;if(old<0||n<0)basalt_panic(1);if(n<=old)return p;if(p){slot=basalt_find(p);if(slot==(size_t)-1)basalt_panic(2);}basalt_checked_bytes(n,sizeof(int));q=(int*)realloc(p,(size_t)n*sizeof(int));if(!q)basalt_panic(6);if(p)basalt_live[slot]=q;else basalt_track(q);memset(q+old,0,(size_t)(n-old)*sizeof(int));return q;}
extern int* payload_int; extern int* payload_name; extern int* payload_string;
extern int* code_kind; extern int* code_value; extern int* input_kind; extern int* input_value;
extern int* source; extern int* sym_start; extern int* sym_len; extern int* sym_hash; extern int* sym_kind; extern int* sym_type; extern int* sym_scope;


#line 146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int next_capacity(int old, int need);

#line 161 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_node(int need);

#line 170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_payload(int need);

#line 180 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_code(int need);

#line 190 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_input(int need);

#line 199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_source(int need);

#line 213 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_sym(int need);

#line 231 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_node(int kind, int a, int b, int c, int value, int aux);

#line 252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_link(int head, int item);

#line 267 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int payload_make_int(int value);

#line 275 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int payload_make_name(int name_id);

#line 283 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int payload_make_string(int string_id);

#line 324 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_snapshot(int need);

#line 342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_bind_clear();

#line 349 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_gen_bind(int need);

#line 354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_bind_find(int name);

#line 359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_bind_add(int name, int ty);

#line 366 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_gen_tuple(int need);

#line 367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_char(int c);

#line 368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_char_text(char c);

#line 369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_text(char* text);

#line 370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_symbol(int id);

#line 378 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_c_symbol(int id);

#line 386 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_mangle_intern(int kind);

#line 390 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_c_symbol(int id);

#line 394 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_uint(int value);

#line 399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_tuple_field_name(int index);

#line 430 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_mangle_type(int ty);

#line 434 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_mangled_type_symbol(int ty);

#line 449 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_add_tuple_type(int ty);

#line 454 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_mangled_function_symbol(int base, int args);

#line 463 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void code_emit(int kind, int value);

#line 467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void code_reset();

#line 474 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_emit_defer(int need);

#line 480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_emit_scope(int need);

#line 486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_emit_loop(int need);

#line 491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_defer_push(int expr);

#line 495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_emit_defer_from(int base);

#line 496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_emit_all_defers();

#line 515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_gen_specs(int need);

#line 526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_gen_struct_state(int need);

#line 532 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_gen_spec_state(int need);

#line 544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_find_spec_index(int decl, int name);

#line 558 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_substitute_type(int ty);

#line 567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_active_param_type(int name);

#line 573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_spec_exists(int kind, int decl, int name);

#line 598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_collect_struct_fields(int decl, int inst);

#line 617 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_add_struct_spec(int ty);

#line 627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_type_has_param(int ty);

#line 661 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_add_fun_spec(int decl, int args);

#line 667 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_collect_type(int ty);

#line 689 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_collect_expr(int id);

#line 705 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_collect_stmt(int id);

#line 713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_alignment(int alignment);

#line 761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_type(int kind, int child, int size);

#line 778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_scalar_kind(int arg);

#line 790 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_scalar_name(int arg);

#line 814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_array_elem_kind(int arg);

#line 839 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_array_elem_name(int arg);

#line 862 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_array_elem_type(int kind, int name);

#line 869 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_array_sizeof(int kind, int name);

#line 888 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_array_value_ptr(int kind, int name, int value);

#line 897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_array_sizeof_node(int ty);

#line 907 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_memory_sizeof(int arg);

#line 1000 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_memory_builtin(int id);

#line 1041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_call_name(int id);

#line 1088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_variant_expr(int id);

#line 1179 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_expr(int id);

#line 1228 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_expr_kind(int id);

#line 1236 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_initializer(int ty, int expr);

#line 1244 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_assignment(int lhs, int rhs);

#line 1258 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int compound_c_operator(int op);

#line 1266 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_compound_assignment(int lhs, int op, int rhs);

#line 1280 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_for_clause(int id);

#line 1297 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_fun_decl(int ty, int name);

#line 1309 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_decl(int ty, int name);

#line 1470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_stmt(int id);

#line 1488 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_unify_formal(int formal, int actual);

#line 1500 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_bind_decl(int decl, int inst);

#line 1507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_struct_decl_specialized(int decl, int inst, int cname);

#line 1520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_function_specialized(int decl, int inst, int cname);

#line 1527 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_match_temp_symbol();

#line 1539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_match_binding(int temp, int variant, int binding, int field);

#line 1584 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_match_stmt(int id);

#line 1603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_tuple_decl(int ty, int name);

#line 1621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_struct_decl(int id);

#line 1636 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_emit_complete_struct(int decl);

#line 1678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_emit_complete_spec(int index);

#line 1695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_emit_complete_type(int ty);

#line 1754 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_tagged_enum_decl(int id);

#line 1780 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_enum_decl(int id);

#line 1788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_extern_param(int ty, int name);

#line 1803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_function_signature(int id);

#line 1811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_prototype(int id);

#line 1833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_function(int id);

#line 1903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_program(int id);

#line 1920 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int build_regression_ast();

#line 1943 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void generator_regression_main();

#line 2046 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void input_reset();

#line 2055 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void input_put(int kind, int value, int text, int pos);

#line 2060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int input_peek();

#line 2065 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int input_payload();

#line 2070 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int input_text_payload();

#line 2077 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int input_take(int kind);

#line 2088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_generic_param(int name);

#line 2103 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_generic_params();

#line 2233 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_type();

#line 2249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_call_args();

#line 2322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_primary();

#line 2343 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_unary();

#line 2364 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_precedence(int kind);

#line 2385 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_operator(int kind);

#line 2399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_compound_operator(int kind);

#line 2406 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_take_compound_operator();

#line 2410 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_compound_assign(int left, int op, int right);

#line 2437 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_expr_prec(int min_prec);

#line 2441 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_expr();

#line 2445 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int clone_for_step(int step);

#line 2469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int lower_for_stmt(int id, int step);

#line 2486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_alignment();

#line 2706 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_stmt();

#line 2722 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_params();

#line 2753 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_struct_decl();

#line 2787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_enum_decl();

#line 2808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_namespace_decl();

#line 2884 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_decl();

#line 2903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_flatten_decl_list(int item);

#line 2923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_program();

#line 3022 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void c_source_reset();

#line 3028 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_c_source(int need);

#line 3033 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void c_source_put(int c);

#line 3041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_source_file_names(int need);

#line 3048 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_source_file_text(int need);

#line 3054 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_path_length(char* path);

#line 3078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_file_intern(char* path);

#line 3123 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_file_intern_include(int*line, int length, int base_id);

#line 3132 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void source_reset();

#line 3141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void source_put(int c);

#line 3149 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int is_space(int c);

#line 3155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int is_digit(int c);

#line 3166 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int is_alpha(int c);

#line 3171 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int is_alnum(int c);

#line 3176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_peek();

#line 3183 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_take();

#line 3247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int span_hash(int start, int length);

#line 3256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int span_equal(int a, int b, int length);

#line 3269 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_lookup(int start, int length, int h);

#line 3281 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_tag_id();

#line 3296 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_qualified(int ns, int name);

#line 3301 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_decl_name(int name);

#line 3307 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_type_name(int name);

#line 3325 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_intern(int start, int length, int kind, int scope);

#line 3335 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_bi(int need);

#line 3351 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void bi_register(char* text, int tc_tag, int flags);

#line 3361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bi_lookup(int name);

#line 3367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bi_tag(int name);

#line 3374 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bi_has_flag(int name, int flag);

#line 3453 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void bi_init();

#line 3563 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int word_code(int start, int length);

#line 3586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void lexer_skip();

#line 3702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int lexer_next();

#line 3744 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void include_process_line(int*line, int length);

#line 3761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void include_expand_handle(int*handle);

#line 3772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void load_source_file(char* path);

#line 3865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int map_token(int k);

#line 3901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void load_tokens_from_file(char* path);

#line 3963 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_tc_vars(int need);

#line 3970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_tc_scopes(int need);

#line 3976 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_enter_scope();

#line 3992 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_leave_scope();

#line 3999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_tc_path(int need);

#line 4007 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_fail(int code);

#line 4015 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_tc_bindings(int need);

#line 4017 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_bind_clear();

#line 4023 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_bind_find(int name);

#line 4036 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_bind_add(int name, int ty);

#line 4043 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_is_integer_kind(int kind);

#line 4048 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_is_numeric_kind(int kind);

#line 4053 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_is_fixed_integer_kind(int kind);

#line 4057 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_is_legacy_integer_kind(int kind);

#line 4073 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_decimal_le(int raw, char* limit);

#line 4088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_literal_fits(int id, int target_kind);

#line 4122 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_type_equal(int a, int b);

#line 4137 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_signature_type(int entry);

#line 4147 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_type_node_from_summary(int kind, int name, int elem_kind, int elem_name);

#line 4159 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_generic_moves_array(int fun_node);

#line 4170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_mark_float_expr(int id, int expected_kind);

#line 4178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_match_generic_call_arg(int formal, int actual, int expr);

#line 4209 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_match_generic(int formal, int actual);

#line 4234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_substitute_type(int ty);

#line 4249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_same(int a_kind, int a_name, int b_kind, int b_name);

#line 4255 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_array_elem_same(int a_kind, int a_name, int b_kind, int b_name);

#line 4287 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_same_full(int a_kind, int a_name, int a_elem_kind, int a_elem_name, int b_kind, int b_name, int b_elem_kind, int b_elem_name);

#line 4295 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_ptr_diff_ok(int a_kind, int a_elem_kind, int a_elem_name, int b_kind, int b_elem_kind, int b_elem_name);

#line 4307 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_suffix_equal(int full, int base);

#line 4316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_struct(int name);

#line 4335 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_struct_ctx(int name, int ns);

#line 4344 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_enum(int name);

#line 4363 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_enum_ctx(int name, int ns);

#line 4370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_generic_arity(int decl);

#line 4377 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_generic_arg_count(int ty);

#line 4383 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_named_exists_ctx(int name, int ns);

#line 4389 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_named_exists(int name);

#line 4415 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_check_type(int ty);

#line 4436 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_cycle_struct(int name);

#line 4446 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_cycle_type(int ty);

#line 4452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_release_name(int name);

#line 4458 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_owned_initializer(int id);

#line 4462 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_is_owner_kind(int kind);

#line 4467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_borrow_conflict(int index);

#line 4476 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_move_var(int index);

#line 4481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_move_value(int id);

#line 4489 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_record_borrow(int destination, int source_index2);

#line 4499 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_require_mutable(int id);

#line 4511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_consume_call(int id);

#line 4537 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_add_var(int name, int kind, int named, int elem_kind, int elem_name, int type_node);

#line 4567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_lookup_var(int name);

#line 4587 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_type_node(int ty);

#line 4605 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_numeric_result_kind(int a, int b);

#line 4621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_integer_result_kind(int a, int b);

#line 4643 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_check_variant(int id);

#line 5099 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_expr(int id);

#line 5108 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_function(int name);

#line 5120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_function_ctx(int name, int ns);

#line 5132 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_enum_value(int name);

#line 5150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_enum_variant(int name);

#line 5157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_match_enum_decl(int ty);

#line 5169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_match_variant_member(int decl, int name);

#line 5178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_match_seen_variant(int head, int member);

#line 5191 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_match_check_arm_bindings(int variant, int bindings);

#line 5222 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_emit_field_type(int id);

#line 5249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_emit_arg_type(int id);

#line 5292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_expr_kind_for_emit(int id);

#line 5418 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_stmt(int id, int expected_kind, int expected_name);

#line 5424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_diag_line(int pos);

#line 5429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_diag_col(int pos);

#line 5471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_diag();

#line 5489 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_check_function_symbols(int root);

#line 5493 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_reserved_function(int name);

#line 5515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_program(int root);

#line 5554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pipeline_main(char* path);

#line 5562 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_symbol(int*out, int id);

#line 5568 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_string(int*out, int id);

#line 5578 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_print_prefix(int*out);

#line 5590 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_int_text(int*out, int value);

#line 5603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_source_filename(int*out, int file_id);

#line 5615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_source_line(int*out, int pos);

#line 5739 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_c_token(int*out, int kind, int value);

#line 5788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_runtime(int*out);

#line 5808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_c_file(char* path);

#line 5822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int main(int argc, char**argv);

#line 10 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_NONE = 0;

#line 11 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_INT = 1;

#line 12 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_BOOL = 2;

#line 13 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_STRING = 3;

#line 14 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_VAR = 4;

#line 15 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_BINOP = 5;

#line 16 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_CALL = 6;

#line 17 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_INDIRECT_CALL = 37;

#line 18 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_COMPOUND_ASSIGN = 38;

#line 19 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_DEREF = 7;

#line 20 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_INDEX = 8;

#line 21 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_ADDRESS = 9;

#line 22 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_LET = 10;

#line 23 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_ASSIGN = 11;

#line 24 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_PRINT = 12;

#line 25 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_IF = 13;

#line 26 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_WHILE = 14;

#line 27 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_BLOCK = 15;

#line 28 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_RETURN = 16;

#line 29 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_GLOBAL = 17;

#line 30 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_PARAM = 18;

#line 31 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_FUNC = 19;

#line 32 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_PROGRAM = 20;

#line 33 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_LIST = 21;

#line 34 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_EXPR = 22;

#line 35 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_BREAK = 23;

#line 36 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_CONTINUE = 24;

#line 37 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_FOR = 25;

#line 38 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_STRUCT = 26;

#line 39 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_ENUM = 27;

#line 40 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_FIELD = 28;

#line 41 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_FIELD_ACCESS = 29;

#line 42 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_CHAR = 30;

#line 43 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_NULL = 31;

#line 44 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_CONST = 32;

#line 45 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_FLOAT = 33;

#line 46 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_EXTERN = 34;

#line 47 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_GENERIC_STRUCT = 35;

#line 48 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_GENERIC_FUNC = 36;

#line 49 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_VARIANT = 39;

#line 50 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_DEFER = 40;

#line 51 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_MATCH = 41;

#line 52 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_MATCH_ARM = 42;

#line 53 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_TUPLE = 43;

#line 54 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int N_TUPLE_BIND = 44;

#line 56 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_ADD = 1;

#line 57 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_SUB = 2;

#line 58 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_MUL = 3;

#line 59 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_DIV = 4;

#line 60 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_EQ = 5;

#line 61 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_NEQ = 6;

#line 62 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_LT = 7;

#line 63 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_GT = 8;

#line 64 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_AND = 9;

#line 65 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_OR = 10;

#line 66 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_CONCAT = 11;

#line 67 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_BITAND = 12;

#line 68 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_BITOR = 13;

#line 69 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_BITXOR = 14;

#line 70 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_SHL = 15;

#line 71 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_SHR = 16;

#line 72 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int OP_MOD = 17;

#line 74 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_INT = 1;

#line 75 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_BOOL = 2;

#line 76 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_STRING = 3;

#line 77 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_VOID = 4;

#line 78 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_PTR = 5;

#line 79 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_ARRAY = 6;

#line 80 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_NAMED = 7;

#line 81 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_CHAR = 8;

#line 82 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_FLOAT = 9;

#line 83 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_DOUBLE = 10;

#line 84 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_FUN = 11;

#line 85 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_DYN_ARRAY = 13;

#line 86 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_PARAM = 14;

#line 87 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_GENERIC = 15;

#line 88 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_LONG = 16;

#line 89 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_LLONG = 17;

#line 90 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_VARIANT = 18;

#line 91 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_U8 = 19;

#line 92 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_U16 = 20;

#line 93 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_U32 = 21;

#line 94 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_U64 = 22;

#line 95 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_I8 = 23;

#line 96 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_I16 = 24;

#line 97 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_I32 = 25;

#line 98 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_I64 = 26;

#line 99 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_USIZE = 27;

#line 100 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int TY_TUPLE = 28;

#line 102 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*node_kind = 0;

#line 103 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*node_a = 0;

#line 104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*node_b = 0;

#line 105 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*node_c = 0;

#line 106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*node_next = 0;

#line 107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*node_value = 0;

#line 108 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*node_aux = 0;

#line 109 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*node_pos = 0;

#line 110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*node_scope = 0;

#line 111 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_parse_mode = 0;

#line 112 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int node_count = 1;

#line 113 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_namespace_scope = 0;

#line 114 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_tag_name = 0;

#line 117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int node_cap = 0;

#line 118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int payload_cap = 0;

#line 119 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int current_source_pos = 0;

#line 120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int code_cap = 0;

#line 121 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pipeline_root = 0;

#line 122 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int input_cap = 0;

#line 123 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_cap = 0;

#line 124 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c_source_cap = 0;

#line 125 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_cap = 0;

#line 126 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*source_file_at = 0;

#line 127 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*source_line_at = 0;

#line 128 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*source_file_name_start = 0;

#line 129 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*source_file_name_len = 0;

#line 130 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*source_file_name_text = 0;

#line 131 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_file_count = 1;

#line 132 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_file_cap = 0;

#line 133 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_file_name_text_len = 0;

#line 134 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_file_name_text_cap = 0;

#line 135 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_active_file = 0;

#line 136 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_active_line = 1;

#line 137 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_source_pos = 0;

#line 138 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_source_epoch = 0;

#line 139 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int emit_pending_space = 0;

#line 256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*payload_int = 0;

#line 257 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*payload_name = 0;

#line 258 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*payload_string = 0;

#line 259 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int payload_count = 1;

#line 288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int C_KW = 1;

#line 289 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int C_IDENT = 2;

#line 290 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int C_INT = 3;

#line 291 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int C_STRING = 4;

#line 292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int C_OP = 5;

#line 293 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int C_PUNCT = 6;

#line 294 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int C_NEWLINE = 7;

#line 295 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int C_RAW = 8;

#line 296 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int C_RAW_U64 = 9;

#line 297 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*code_kind = 0;

#line 298 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*code_value = 0;

#line 299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*code_pos = 0;

#line 300 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*code_epoch = 0;

#line 301 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int code_count = 0;

#line 302 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int emit_for_step = 0;

#line 303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*emit_defer_expr = 0;

#line 304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*emit_defer_scope_start = 0;

#line 305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*emit_scope_start = 0;

#line 306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int emit_defer_count = 0;

#line 307 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int emit_defer_cap = 0;

#line 308 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int emit_scope_depth = 0;

#line 309 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int emit_scope_cap = 0;

#line 310 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*emit_loop_base = 0;

#line 311 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int emit_loop_depth = 0;

#line 312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int emit_loop_cap = 0;

#line 314 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*snapshot_kind = 0;

#line 315 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*snapshot_value = 0;

#line 316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int snapshot_cap = 0;

#line 329 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*gen_bind_name = 0;

#line 330 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*gen_bind_type = 0;

#line 331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_bind_count = 0;

#line 332 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_bind_cap = 0;

#line 333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_active_function = 0;

#line 334 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_match_serial = 0;

#line 335 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*gen_tuple_type = 0;

#line 336 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*gen_tuple_name = 0;

#line 337 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_tuple_count = 0;

#line 338 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_tuple_cap = 0;

#line 339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_mangle_start = 0;

#line 340 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_mangle_len = 0;

#line 498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*gen_spec_kind = 0;

#line 499 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*gen_spec_decl = 0;

#line 500 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*gen_spec_type = 0;

#line 501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*gen_spec_name = 0;

#line 502 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_spec_count = 0;

#line 503 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_spec_cap = 0;

#line 504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_name_override = 0;

#line 505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_debug = 0;

#line 516 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*gen_struct_state = 0;

#line 517 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_struct_state_cap = 0;

#line 518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*gen_spec_state = 0;

#line 519 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_spec_state_cap = 0;

#line 1946 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_EOF = 0;

#line 1947 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_LET = 1;

#line 1948 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_FUNC = 2;

#line 1949 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_ID = 3;

#line 1950 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_INT = 4;

#line 1951 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_STRING = 5;

#line 1952 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TRUE = 6;

#line 1953 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_FALSE = 7;

#line 1954 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_RETURN = 8;

#line 1955 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_WHILE = 9;

#line 1956 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_FOR = 10;

#line 1957 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_BREAK = 11;

#line 1958 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_CONTINUE = 12;

#line 1959 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_IF = 13;

#line 1960 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_THEN = 14;

#line 1961 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_ELSE = 15;

#line 1962 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_PRINT = 16;

#line 1963 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TINT = 17;

#line 1964 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TBOOL = 18;

#line 1965 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TSTRING = 19;

#line 1966 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TVOID = 20;

#line 1967 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_PLUS = 21;

#line 1968 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_MINUS = 22;

#line 1969 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_STAR = 23;

#line 1970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_DIVIDE = 24;

#line 1971 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_CONCAT = 25;

#line 1972 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_AND_AND = 26;

#line 1973 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_OR_OR = 27;

#line 1974 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_EQUAL = 28;

#line 1975 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_EQEQ = 29;

#line 1976 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_NEQ = 30;

#line 1977 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_LT = 31;

#line 1978 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_GT = 32;

#line 1979 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_COLON = 33;

#line 1980 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_LPAREN = 34;

#line 1981 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_RPAREN = 35;

#line 1982 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_LBRACE = 36;

#line 1983 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_RBRACE = 37;

#line 1984 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_SEMI = 38;

#line 1985 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_COMMA = 39;

#line 1986 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_AMP = 40;

#line 1987 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_LBRACK = 41;

#line 1988 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_RBRACK = 42;

#line 1989 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_STRUCT = 43;

#line 1990 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_ENUM = 44;

#line 1991 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_DOT = 45;

#line 1992 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_CHAR = 46;

#line 1993 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_NULL = 47;

#line 1994 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_CONST = 48;

#line 1995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TCHAR = 49;

#line 1996 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_FLOAT = 50;

#line 1997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TDOUBLE = 51;

#line 1998 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_BITOR = 52;

#line 1999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_BITXOR = 53;

#line 2000 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_BITNOT = 54;

#line 2001 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_SHL = 55;

#line 2002 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_SHR = 56;

#line 2003 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_FN = 57;

#line 2004 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_EXTERN = 58;

#line 2005 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_ARRAY = 59;

#line 2006 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_NAMESPACE = 60;

#line 2007 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_SCOPE = 61;

#line 2008 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_MOD = 62;

#line 2009 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_PLUS_EQ = 63;

#line 2010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_MINUS_EQ = 64;

#line 2011 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_STAR_EQ = 65;

#line 2012 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_DIV_EQ = 66;

#line 2013 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_MOD_EQ = 67;

#line 2014 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_AMP_EQ = 68;

#line 2015 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_BITOR_EQ = 69;

#line 2016 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_BITXOR_EQ = 70;

#line 2017 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_SHL_EQ = 71;

#line 2018 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_SHR_EQ = 72;

#line 2019 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TLONG = 73;

#line 2020 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_ALIGNAS = 74;

#line 2021 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TU8 = 75;

#line 2022 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TU16 = 76;

#line 2023 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TU32 = 77;

#line 2024 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TU64 = 78;

#line 2025 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TI8 = 79;

#line 2026 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TI16 = 80;

#line 2027 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TI32 = 81;

#line 2028 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TI64 = 82;

#line 2029 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_TUSIZE = 83;

#line 2030 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_DEFER = 84;

#line 2031 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_MATCH = 85;

#line 2032 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int T_FATARROW = 86;

#line 2034 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*input_kind = 0;

#line 2035 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*input_value = 0;

#line 2036 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*input_text = 0;

#line 2037 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*input_source_pos = 0;

#line 2038 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int input_count = 0;

#line 2039 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int input_pos = 0;

#line 2040 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int debug_tokens = 0;

#line 2041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int for_step_context = 0;

#line 2079 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_generic_scope = 0;

#line 2925 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_EOF = 0;

#line 2926 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_ID = 1;

#line 2927 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_INT = 2;

#line 2928 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_FUNC = 3;

#line 2929 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_LET = 4;

#line 2930 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_PRINT = 5;

#line 2931 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_RETURN = 6;

#line 2932 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_IF = 7;

#line 2933 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_ELSE = 8;

#line 2934 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_WHILE = 9;

#line 2935 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TRUE = 10;

#line 2936 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_FALSE = 11;

#line 2937 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TINT = 12;

#line 2938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TBOOL = 13;

#line 2939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TSTRING = 14;

#line 2940 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_STRING = 35;

#line 2941 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TVOID = 15;

#line 2942 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_THEN = 37;

#line 2943 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_AMP = 34;

#line 2944 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_PLUS = 16;

#line 2945 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_MINUS = 17;

#line 2946 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_STAR = 18;

#line 2947 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_DIV = 19;

#line 2948 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_EQ = 20;

#line 2949 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_EQEQ = 21;

#line 2950 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_NEQ = 22;

#line 2951 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_LT = 23;

#line 2952 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_GT = 24;

#line 2953 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_LPAREN = 25;

#line 2954 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_RPAREN = 26;

#line 2955 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_LBRACE = 27;

#line 2956 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_RBRACE = 28;

#line 2957 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_COLON = 29;

#line 2958 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_SEMI = 30;

#line 2959 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_COMMA = 31;

#line 2960 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_LBRACK = 32;

#line 2961 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_RBRACK = 33;

#line 2962 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_FOR = 38;

#line 2963 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_BREAK = 39;

#line 2964 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_CONTINUE = 40;

#line 2965 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_CONCAT = 41;

#line 2966 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_AND = 42;

#line 2967 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_OR = 43;

#line 2968 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_STRUCT = 44;

#line 2969 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_ENUM = 45;

#line 2970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_DOT = 46;

#line 2971 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_CHAR = 47;

#line 2972 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_NULL = 48;

#line 2973 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_CONST = 49;

#line 2974 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TCHAR = 50;

#line 2975 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_FLOAT = 51;

#line 2976 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TFLOAT = 52;

#line 2977 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TDOUBLE = 53;

#line 2978 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_BITOR = 54;

#line 2979 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_BITXOR = 55;

#line 2980 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_BITNOT = 56;

#line 2981 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_SHL = 57;

#line 2982 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_SHR = 58;

#line 2983 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_FN = 59;

#line 2984 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_EXTERN = 60;

#line 2985 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_ARRAY = 61;

#line 2986 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_NAMESPACE = 62;

#line 2987 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_SCOPE = 63;

#line 2988 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_MOD = 64;

#line 2989 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TLONG = 65;

#line 2990 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_ALIGNAS = 76;

#line 2991 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TU8 = 77;

#line 2992 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TU16 = 78;

#line 2993 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TU32 = 79;

#line 2994 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TU64 = 80;

#line 2995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TI8 = 81;

#line 2996 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TI16 = 82;

#line 2997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TI32 = 83;

#line 2998 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TI64 = 84;

#line 2999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_TUSIZE = 85;

#line 3000 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_PLUS_EQ = 66;

#line 3001 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_MINUS_EQ = 67;

#line 3002 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_STAR_EQ = 68;

#line 3003 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_DIV_EQ = 69;

#line 3004 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_MOD_EQ = 70;

#line 3005 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_AMP_EQ = 71;

#line 3006 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_BITOR_EQ = 72;

#line 3007 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_BITXOR_EQ = 73;

#line 3008 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_SHL_EQ = 74;

#line 3009 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_SHR_EQ = 75;

#line 3010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_DEFER = 86;

#line 3011 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_MATCH = 87;

#line 3012 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int L_FATARROW = 88;

#line 3014 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*source = 0;

#line 3015 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_len = 0;

#line 3016 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_pos = 0;

#line 3017 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*c_source = 0;

#line 3018 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c_source_len = 0;

#line 3019 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int include_ok = 1;

#line 3020 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int include_line_cap = 4096;

#line 3185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*sym_start = 0;

#line 3186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*sym_len = 0;

#line 3187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*sym_hash = 0;

#line 3188 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*sym_kind = 0;

#line 3189 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*sym_type = 0;

#line 3190 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*sym_elem_kind = 0;

#line 3191 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*sym_elem_name = 0;

#line 3192 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*sym_scope = 0;

#line 3193 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_count = 1;

#line 3194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_text_len = 0;

#line 3195 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_NONE = 0;

#line 3196 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_VOID = 1;

#line 3197 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_INT = 2;

#line 3198 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_STRING = 3;

#line 3199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_PTR_INT = 4;

#line 3200 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_PTR_VOID = 5;

#line 3201 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_MEM_ALLOC = 6;

#line 3202 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_MEM_ALLOC_ALIGNED = 30;

#line 3203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_MEM_RESIZE = 7;

#line 3204 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_MEM_FREE = 8;

#line 3205 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_READ_LINE = 9;

#line 3206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_READ_INT = 10;

#line 3207 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_WRITE_STRING = 11;

#line 3208 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_WRITE_LINE = 12;

#line 3209 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_WRITE_INT = 13;

#line 3210 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_WRITE_CHAR = 14;

#line 3211 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_IO_STATUS = 15;

#line 3212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_ATOMIC_MAKE = 16;

#line 3213 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_ATOMIC_LOAD = 17;

#line 3214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_ATOMIC_STORE = 18;

#line 3215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_ATOMIC_FETCH_ADD = 19;

#line 3216 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_ATOMIC_CAS = 20;

#line 3217 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_ATOMIC_FREE = 21;

#line 3218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_CHANNEL_MAKE = 22;

#line 3219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_CHANNEL_SEND = 23;

#line 3220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_CHANNEL_RECV = 24;

#line 3221 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_CHANNEL_CLOSE = 25;

#line 3222 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_CHANNEL_FREE = 26;

#line 3223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_THREAD_SPAWN = 27;

#line 3224 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_THREAD_JOIN = 28;

#line 3225 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_TC_THREAD_YIELD = 29;

#line 3226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_FLAG_RESERVED = 1;

#line 3227 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_FLAG_OWNED = 2;

#line 3228 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_FLAG_CONSUME = 4;

#line 3229 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_FLAG_DYNFIELD = 16;

#line 3230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int BI_FLAG_MAIN = 32;

#line 3231 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bi_count = 0;

#line 3232 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bi_cap = 0;

#line 3233 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*bi_name = 0;

#line 3234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*bi_len = 0;

#line 3235 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*bi_tc = 0;

#line 3236 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*bi_flags = 0;

#line 3565 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tok_kind = 0;

#line 3566 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tok_value = 0;

#line 3567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tok_text = 0;

#line 3568 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tok_start = 0;

#line 3569 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tok_length = 0;

#line 3905 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_root = 0;

#line 3906 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_ok = 1;

#line 3907 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_error_code = 0;

#line 3908 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_error_symbol = 0;

#line 3909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_error_pos = 0;

#line 3910 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_name = 0;

#line 3911 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_kind = 0;

#line 3912 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_elem_kind = 0;

#line 3913 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_elem_name = 0;

#line 3914 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_expected_elem_kind = 0;

#line 3915 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_expected_elem_name = 0;

#line 3916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_var_name = 0;

#line 3917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_var_kind = 0;

#line 3918 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_var_named = 0;

#line 3919 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_var_elem_kind = 0;

#line 3920 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_var_elem_name = 0;

#line 3921 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_var_type = 0;

#line 3922 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_var_owned = 0;

#line 3923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_var_moved = 0;

#line 3924 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_var_borrow_count = 0;

#line 3925 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_var_borrow_source = 0;

#line 3926 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_last_var_type = 0;

#line 3927 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_last_var_owned = 0;

#line 3928 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_last_var_moved = 0;

#line 3929 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_last_var_index = 0;

#line 3930 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_expr_borrow_source = (0-1);

#line 3931 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_var_count = 0;

#line 3932 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_global_count = 0;

#line 3933 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_var_cap = 0;

#line 3934 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_scope_start = 0;

#line 3935 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_scope_count = 0;

#line 3936 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_scope_cap = 0;

#line 3937 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_path_name = 0;

#line 3938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_path_count = 0;

#line 3939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_path_cap = 0;

#line 3940 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_loop_depth = 0;

#line 3941 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_result_type = 0;

#line 3942 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_variant_enum = 0;

#line 3943 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_variant_member = 0;

#line 3944 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_bind_name = 0;

#line 3945 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*tc_bind_type = 0;

#line 3946 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_bind_count = 0;

#line 3947 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_bind_cap = 0;

#line 146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int next_capacity(int old, int need)
#line 146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 142 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = old;

#line 143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((n<16))
#line 143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
n = 16;
else
#line 143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((n<(need+1)))
#line 144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
n = (n*2);
}

#line 145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return n;
}

#line 161 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_node(int need)
#line 161 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 149 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<node_cap))
#line 149 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 149 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(node_cap, need);

#line 151 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_kind = grow_ints(node_kind, node_cap, n);

#line 152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_a = grow_ints(node_a, node_cap, n);

#line 153 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_b = grow_ints(node_b, node_cap, n);

#line 154 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_c = grow_ints(node_c, node_cap, n);

#line 155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_next = grow_ints(node_next, node_cap, n);

#line 156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_value = grow_ints(node_value, node_cap, n);

#line 157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux = grow_ints(node_aux, node_cap, n);

#line 158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_pos = grow_ints(node_pos, node_cap, n);

#line 159 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_scope = grow_ints(node_scope, node_cap, n);

#line 160 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_cap = n;
}

#line 170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_payload(int need)
#line 170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<payload_cap))
#line 164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 165 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(payload_cap, need);

#line 166 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_int = grow_ints(payload_int, payload_cap, n);

#line 167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_name = grow_ints(payload_name, payload_cap, n);

#line 168 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_string = grow_ints(payload_string, payload_cap, n);

#line 169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_cap = n;
}

#line 180 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_code(int need)
#line 180 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 173 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<code_cap))
#line 173 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 173 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 174 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(code_cap, need);

#line 175 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_kind = grow_ints(code_kind, code_cap, n);

#line 176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_value = grow_ints(code_value, code_cap, n);

#line 177 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_pos = grow_ints(code_pos, code_cap, n);

#line 178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_epoch = grow_ints(code_epoch, code_cap, n);

#line 179 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_cap = n;
}

#line 190 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_input(int need)
#line 190 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 183 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<input_cap))
#line 183 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 183 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 184 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(input_cap, need);

#line 185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_kind = grow_ints(input_kind, input_cap, n);

#line 186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_value = grow_ints(input_value, input_cap, n);

#line 187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_text = grow_ints(input_text, input_cap, n);

#line 188 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_source_pos = grow_ints(input_source_pos, input_cap, n);

#line 189 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_cap = n;
}

#line 199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_source(int need)
#line 199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 193 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<source_cap))
#line 193 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 193 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(source_cap, need);

#line 195 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source = grow_ints(source, source_cap, n);

#line 196 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_at = grow_ints(source_file_at, source_cap, n);

#line 197 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_line_at = grow_ints(source_line_at, source_cap, n);

#line 198 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_cap = n;
}

#line 213 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_sym(int need)
#line 213 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 202 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<sym_cap))
#line 202 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 202 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(sym_cap, need);

#line 204 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_start = grow_ints(sym_start, sym_cap, n);

#line 205 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_len = grow_ints(sym_len, sym_cap, n);

#line 206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_hash = grow_ints(sym_hash, sym_cap, n);

#line 207 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_kind = grow_ints(sym_kind, sym_cap, n);

#line 208 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_type = grow_ints(sym_type, sym_cap, n);

#line 209 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_elem_kind = grow_ints(sym_elem_kind, sym_cap, n);

#line 210 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_elem_name = grow_ints(sym_elem_name, sym_cap, n);

#line 211 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_scope = grow_ints(sym_scope, sym_cap, n);

#line 212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_cap = n;
}

#line 231 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_node(int kind, int a, int b, int c, int value, int aux)
#line 231 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 216 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int id = node_count;

#line 217 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int parse_pos = current_source_pos;

#line 218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((ast_parse_mode==1)&&(input_pos>0))&&(input_pos<(input_count+1))))
#line 218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
parse_pos = input_source_pos[(input_pos-1)];
else
#line 218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_node(id);

#line 220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_kind[id] = kind;

#line 221 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_a[id] = a;

#line 222 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_b[id] = b;

#line 223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_c[id] = c;

#line 224 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_next[id] = 0;

#line 225 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_value[id] = value;

#line 226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux[id] = aux;

#line 227 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_pos[id] = parse_pos;

#line 228 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_scope[id] = ast_namespace_scope;

#line 229 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_count = (node_count+1);

#line 230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
}

#line 252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_link(int head, int item)
#line 252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((head==0))
#line 234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return item;
else
#line 234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 235 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = head;

#line 245 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 245 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((p==item))
#line 243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 238 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int copy = ast_node(node_kind[item], node_a[item], node_b[item], node_c[item], node_value[item], node_aux[item]);

#line 239 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_pos[copy] = node_pos[item];

#line 240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_scope[copy] = node_scope[item];

#line 241 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = copy;

#line 242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
break;
}
else
#line 243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 244 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 246 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = head;

#line 249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((node_next[p]!=0))
#line 249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 248 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 250 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_next[p] = item;

#line 251 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return head;
}

#line 267 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int payload_make_int(int value)
#line 267 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 262 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int id = payload_count;

#line 263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_payload(id);

#line 264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_int[id] = value;

#line 265 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_count = (payload_count+1);

#line 266 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
}

#line 275 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int payload_make_name(int name_id)
#line 275 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 270 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int id = payload_count;

#line 271 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_payload(id);

#line 272 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_name[id] = name_id;

#line 273 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_count = (payload_count+1);

#line 274 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
}

#line 283 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int payload_make_string(int string_id)
#line 283 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 278 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int id = payload_count;

#line 279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_payload(id);

#line 280 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_string[id] = string_id;

#line 281 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_count = (payload_count+1);

#line 282 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
}

#line 324 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_snapshot(int need)
#line 324 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<snapshot_cap))
#line 319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 320 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(snapshot_cap, need);

#line 321 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
snapshot_kind = grow_ints(snapshot_kind, snapshot_cap, n);

#line 322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
snapshot_value = grow_ints(snapshot_value, snapshot_cap, n);

#line 323 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
snapshot_cap = n;
}

#line 342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_bind_clear()
#line 342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_count = 0;
}

#line 349 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_gen_bind(int need)
#line 349 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 344 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<gen_bind_cap))
#line 344 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 344 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 345 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(gen_bind_cap, need);

#line 346 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name = grow_ints(gen_bind_name, gen_bind_cap, n);

#line 347 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type = grow_ints(gen_bind_type, gen_bind_cap, n);

#line 348 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_cap = n;
}

#line 354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_bind_find(int name)
#line 354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 351 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<gen_bind_count))
#line 352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_bind_name[i]==name))
#line 352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_bind_type[i];
else
#line 352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 353 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_bind_add(int name, int ty)
#line 359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 356 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<gen_bind_count))
#line 357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_bind_name[i]==name))
#line 357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[i] = ty;

#line 357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 358 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_bind(gen_bind_count);

#line 358 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[gen_bind_count] = name;

#line 358 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[gen_bind_count] = ty;

#line 358 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_count = (gen_bind_count+1);
}

#line 366 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_gen_tuple(int need)
#line 366 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<gen_tuple_cap))
#line 361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 362 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(gen_tuple_cap, need);

#line 363 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_tuple_type = grow_ints(gen_tuple_type, gen_tuple_cap, n);

#line 364 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_tuple_name = grow_ints(gen_tuple_name, gen_tuple_cap, n);

#line 365 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_tuple_cap = n;
}

#line 367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_char(int c)
#line 367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pos = ((source_len+sym_text_len)+gen_mangle_len);

#line 367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source(pos);

#line 367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source[pos] = c;

#line 367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_len = (gen_mangle_len+1);
}

#line 368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_char_text(char c)
#line 368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pos = ((source_len+sym_text_len)+gen_mangle_len);

#line 368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source(pos);

#line 368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source[pos] = c;

#line 368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_len = (gen_mangle_len+1);
}

#line 369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_text(char* text)
#line 369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((text[i]!=0))
#line 369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_char_text(text[i]);

#line 369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}
}

#line 370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_symbol(int id)
#line 370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<sym_len[id]))
#line 370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_char(source[(sym_start[id]+i)]);

#line 370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}
}

#line 378 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_c_symbol(int id)
#line 378 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 372 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 377 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<sym_len[id]))
#line 377 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 374 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c = source[(sym_start[id]+i)];

#line 376 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((c==58)&&((i+1)<sym_len[id]))&&(source[((sym_start[id]+i)+1)]==58)))
#line 375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_char(95);

#line 375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_char(95);

#line 375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+2);
}
else
#line 376 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 376 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_char(c);

#line 376 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}
}
}

#line 386 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_mangle_intern(int kind)
#line 386 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 380 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int h = span_hash(gen_mangle_start, gen_mangle_len);

#line 381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int old = sym_lookup(gen_mangle_start, gen_mangle_len, h);

#line 382 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((old!=0))
#line 382 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return old;
else
#line 382 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 383 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int id = sym_intern(gen_mangle_start, gen_mangle_len, kind, 0);

#line 384 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_text_len = (sym_text_len+gen_mangle_len);

#line 385 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
}

#line 390 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_c_symbol(int id)
#line 390 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 388 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_start = (source_len+sym_text_len);

#line 388 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_len = 0;

#line 388 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_c_symbol(id);

#line 389 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_mangle_intern(L_ID);
}

#line 394 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_append_uint(int value)
#line 394 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value>9))
#line 392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_uint((value/10));
else
#line 392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 393 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_char((48+(value%10)));
}

#line 399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_tuple_field_name(int index)
#line 399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 396 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_start = (source_len+sym_text_len);

#line 396 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_len = 0;

#line 397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("item");

#line 397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_uint(index);

#line 398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_mangle_intern(L_ID);
}

#line 430 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_mangle_type(int ty)
#line 430 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("void");

#line 401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_PARAM))
#line 402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int b = gen_bind_find(node_value[ty]);

#line 402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((b!=0))
#line 402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_type(b);

#line 402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_c_symbol(node_value[ty]);

#line 402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_INT))
#line 403 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("int");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_BOOL))
#line 404 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("bool");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_STRING))
#line 405 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("char_ptr");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_CHAR))
#line 406 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("char");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_FLOAT))
#line 407 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("float");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_DOUBLE))
#line 408 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("double");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_U8))
#line 409 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("u8");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_U16))
#line 410 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("u16");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_U32))
#line 411 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("u32");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_U64))
#line 412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("u64");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_I8))
#line 413 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("i8");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_I16))
#line 414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("i16");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_I32))
#line 415 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("i32");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_I64))
#line 416 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("i64");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_USIZE))
#line 417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("usize");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_VOID))
#line 418 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("void");
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_NAMED))
#line 419 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_c_symbol(node_value[ty]);
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_PTR))
#line 420 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 420 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("ptr_");

#line 420 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_type(node_a[ty]);
}
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[ty]==TY_ARRAY)||(node_kind[ty]==TY_DYN_ARRAY)))
#line 421 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 421 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("array_");

#line 421 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_type(node_a[ty]);
}
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_GENERIC))
#line 425 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_c_symbol(node_value[ty]);

#line 422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("__");

#line 423 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = node_a[ty];

#line 424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((a!=0))
#line 424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_type(a);

#line 424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[a]!=0))
#line 424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("__");
else
#line 424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}
}
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_TUPLE))
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 426 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("tuple");

#line 427 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int t = node_a[ty];

#line 428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((t!=0))
#line 428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("__");

#line 428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_type(t);

#line 428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
t = node_next[t];
}
}
else
#line 429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("int");
}

#line 434 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_mangled_type_symbol(int ty)
#line 434 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 432 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_start = (source_len+sym_text_len);

#line 432 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_len = 0;

#line 432 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_type(ty);

#line 433 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_mangle_intern(0);
}

#line 449 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_add_tuple_type(int ty)
#line 449 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 436 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ty==0)||(node_kind[ty]!=TY_TUPLE)))
#line 436 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 436 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 437 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = gen_mangled_type_symbol(ty);

#line 438 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 442 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<gen_tuple_count))
#line 442 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 440 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_tuple_name[i]==name))
#line 440 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 440 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 441 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 443 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[ty];

#line 444 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 444 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 444 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(item);

#line 444 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 445 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_tuple(gen_tuple_count);

#line 446 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_tuple_type[gen_tuple_count] = ty;

#line 447 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_tuple_name[gen_tuple_count] = name;

#line 448 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_tuple_count = (gen_tuple_count+1);
}

#line 454 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_mangled_function_symbol(int base, int args)
#line 454 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 451 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_start = (source_len+sym_text_len);

#line 451 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_len = 0;

#line 451 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_c_symbol(base);

#line 451 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("__");

#line 452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = args;

#line 452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((a!=0))
#line 452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_type(a);

#line 452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[a]!=0))
#line 452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("__");
else
#line 452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}

#line 453 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_mangle_intern(0);
}

#line 463 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void code_emit(int kind, int value)
#line 463 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 457 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_code(code_count);

#line 458 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_kind[code_count] = kind;

#line 459 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_value[code_count] = value;

#line 460 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_pos[code_count] = gen_source_pos;

#line 461 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_epoch[code_count] = gen_source_epoch;

#line 462 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_count = (code_count+1);
}

#line 467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void code_reset()
#line 467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 466 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_count = 0;
}

#line 474 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_emit_defer(int need)
#line 474 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<emit_defer_cap))
#line 469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(emit_defer_cap, need);

#line 471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_defer_expr = grow_ints(emit_defer_expr, emit_defer_cap, n);

#line 472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_defer_scope_start = grow_ints(emit_defer_scope_start, emit_defer_cap, n);

#line 473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_defer_cap = n;
}

#line 480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_emit_scope(int need)
#line 480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 476 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<emit_scope_cap))
#line 476 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 476 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 477 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(emit_scope_cap, need);

#line 478 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_scope_start = grow_ints(emit_scope_start, emit_scope_cap, n);

#line 479 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_scope_cap = n;
}

#line 486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_emit_loop(int need)
#line 486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<emit_loop_cap))
#line 482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(emit_loop_cap, need);

#line 484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_loop_base = grow_ints(emit_loop_base, emit_loop_cap, n);

#line 485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_loop_cap = n;
}

#line 491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_defer_push(int expr)
#line 491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 488 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_emit_defer(emit_defer_count);

#line 489 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_defer_expr[emit_defer_count] = expr;

#line 490 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_defer_count = (emit_defer_count+1);
}

#line 495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_emit_defer_from(int base)
#line 495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 493 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = (emit_defer_count-1);

#line 494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((i+1)>base))
#line 494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(emit_defer_expr[i]);

#line 494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i-1);
}
}

#line 496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_emit_all_defers()
#line 496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_defer_from(0);
}

#line 515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_gen_specs(int need)
#line 515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<gen_spec_cap))
#line 508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 509 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(gen_spec_cap, need);

#line 510 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_kind = grow_ints(gen_spec_kind, gen_spec_cap, n);

#line 511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_decl = grow_ints(gen_spec_decl, gen_spec_cap, n);

#line 512 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_type = grow_ints(gen_spec_type, gen_spec_cap, n);

#line 513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_name = grow_ints(gen_spec_name, gen_spec_cap, n);

#line 514 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_cap = n;
}

#line 526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_gen_struct_state(int need)
#line 526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 522 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<gen_struct_state_cap))
#line 522 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 522 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 523 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(gen_struct_state_cap, need);

#line 524 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_struct_state = grow_ints(gen_struct_state, gen_struct_state_cap, n);

#line 525 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_struct_state_cap = n;
}

#line 532 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_gen_spec_state(int need)
#line 532 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 528 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<gen_spec_state_cap))
#line 528 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 528 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 529 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(gen_spec_state_cap, need);

#line 530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_state = grow_ints(gen_spec_state, gen_spec_state_cap, n);

#line 531 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_state_cap = n;
}

#line 544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_find_spec_index(int decl, int name)
#line 544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 534 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 542 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<gen_spec_count))
#line 542 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_kind[i]==1))
#line 540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_decl[i]==decl))
#line 539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 538 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_name[i]==name))
#line 538 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return i;
else
#line 538 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 541 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 543 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
}

#line 558 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_substitute_type(int ty)
#line 558 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 547 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 547 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 547 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_PARAM))
#line 548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int b = gen_bind_find(node_value[ty]);

#line 548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((b!=0))
#line 548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_substitute_type(b);
else
#line 548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_PARAM, node_a[ty], node_b[ty], node_c[ty], node_value[ty], node_aux[ty]);
}
else
#line 548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_PTR))
#line 549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_PTR, gen_substitute_type(node_a[ty]), 0, 0, 0, 0);
else
#line 549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_ARRAY))
#line 550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_ARRAY, gen_substitute_type(node_a[ty]), 0, 0, node_value[ty], 0);
else
#line 550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_DYN_ARRAY))
#line 551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_DYN_ARRAY, gen_substitute_type(node_a[ty]), 0, 0, 0, 0);
else
#line 551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_GENERIC))
#line 556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 553 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int args = 0;

#line 553 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_a[ty];

#line 554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int q = gen_substitute_type(p);

#line 554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((args==0))
#line 554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = q;
else
#line 554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = ast_link(args, q);

#line 554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 555 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_GENERIC, args, 0, 0, node_value[ty], 0);
}
else
#line 556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(node_kind[ty], node_a[ty], node_b[ty], node_c[ty], node_value[ty], node_aux[ty]);
}

#line 567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_active_param_type(int name)
#line 567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_active_function==0))
#line 560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 561 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_c[gen_active_function];

#line 565 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 565 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 563 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[p]==name))
#line 563 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_b[p];
else
#line 563 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 564 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 566 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_spec_exists(int kind, int decl, int name)
#line 573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 570 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 571 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<gen_spec_count))
#line 571 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 571 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((gen_spec_kind[i]==kind)&&(gen_spec_decl[i]==decl))&&(gen_spec_name[i]==name)))
#line 571 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 571 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 571 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 572 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_collect_struct_fields(int decl, int inst)
#line 598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 575 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int saved_count = gen_bind_count;

#line 576 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_bind((saved_count+saved_count));

#line 577 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int save_i = 0;

#line 582 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((save_i<saved_count))
#line 582 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 579 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[(saved_count+save_i)] = gen_bind_name[save_i];

#line 580 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[(saved_count+save_i)] = gen_bind_type[save_i];

#line 581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
save_i = (save_i+1);
}

#line 583 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_decl(decl, inst);

#line 584 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = node_a[decl];

#line 589 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((field!=0))
#line 589 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field_type = gen_substitute_type(node_b[field]);

#line 587 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(field_type);

#line 588 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
field = node_next[field];
}

#line 590 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_clear();

#line 591 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int restore_i = 0;

#line 596 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((restore_i<saved_count))
#line 596 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 593 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[restore_i] = gen_bind_name[(saved_count+restore_i)];

#line 594 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[restore_i] = gen_bind_type[(saved_count+restore_i)];

#line 595 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
restore_i = (restore_i+1);
}

#line 597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_count = saved_count;
}

#line 617 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_add_struct_spec(int ty)
#line 617 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 601 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int q = gen_substitute_type(ty);

#line 602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((q==0)||(node_kind[q]!=TY_GENERIC)))
#line 602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int decl = tc_find_struct(node_value[q]);

#line 603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((decl==0))
#line 603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 604 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = gen_mangled_type_symbol(q);

#line 605 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_exists(1, decl, name)==1))
#line 605 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 605 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 606 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int slot = gen_spec_count;

#line 607 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_specs(gen_spec_count);

#line 608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_kind[slot] = 1;

#line 608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_decl[slot] = decl;

#line 608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_type[slot] = q;

#line 608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_name[slot] = name;

#line 608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_count = (gen_spec_count+1);

#line 609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = node_a[q];

#line 609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((a!=0))
#line 609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(a);

#line 609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}

#line 610 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_struct_fields(decl, q);

#line 611 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int last = (gen_spec_count-1);

#line 615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((slot<last))
#line 615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_kind[slot] = gen_spec_kind[(slot+1)];

#line 613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_decl[slot] = gen_spec_decl[(slot+1)];

#line 613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_type[slot] = gen_spec_type[(slot+1)];

#line 613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_name[slot] = gen_spec_name[(slot+1)];

#line 614 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
slot = (slot+1);
}

#line 616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_kind[last] = 1;

#line 616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_decl[last] = decl;

#line 616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_type[last] = q;

#line 616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_name[last] = name;
}

#line 627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_type_has_param(int ty)
#line 627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 620 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_PARAM))
#line 620 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 620 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[ty]==TY_PTR)||(node_kind[ty]==TY_ARRAY))||(node_kind[ty]==TY_DYN_ARRAY)))
#line 621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_type_has_param(node_a[ty]);
else
#line 621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 625 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_GENERIC))
#line 625 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 623 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = node_a[ty];

#line 624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((a!=0))
#line 624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_type_has_param(a)==1))
#line 624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}
}
else
#line 625 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 626 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 661 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_add_fun_spec(int decl, int args)
#line 661 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 630 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int saved_count = gen_bind_count;

#line 631 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_bind((saved_count+saved_count));

#line 632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int save_i = 0;

#line 637 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((save_i<saved_count))
#line 637 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 634 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[(saved_count+save_i)] = gen_bind_name[save_i];

#line 635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[(saved_count+save_i)] = gen_bind_type[save_i];

#line 636 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
save_i = (save_i+1);
}

#line 638 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int actual = 0;

#line 638 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = args;

#line 639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int q = gen_substitute_type(p);

#line 639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((actual==0))
#line 639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
actual = q;
else
#line 639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
actual = ast_link(actual, q);

#line 639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 640 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_decl(decl, actual);

#line 641 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int typeargs = 0;

#line 641 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tp = node_aux[decl];

#line 648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((tp!=0))
#line 648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 643 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bt = gen_bind_find(node_a[tp]);

#line 644 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bt==0))
#line 644 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bt = ast_node(TY_PARAM, 0, 0, 0, node_a[tp], 0);
else
#line 644 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 645 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int cq = gen_substitute_type(bt);

#line 646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((typeargs==0))
#line 646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
typeargs = cq;
else
#line 646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
typeargs = ast_link(typeargs, cq);

#line 647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tp = node_next[tp];
}

#line 649 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_clear();

#line 650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int restore_i = 0;

#line 655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((restore_i<saved_count))
#line 655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[restore_i] = gen_bind_name[(saved_count+restore_i)];

#line 653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[restore_i] = gen_bind_type[(saved_count+restore_i)];

#line 654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
restore_i = (restore_i+1);
}

#line 656 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_count = saved_count;

#line 657 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = gen_mangled_function_symbol(node_value[decl], typeargs);

#line 658 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_exists(2, decl, name)==1))
#line 658 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 658 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_specs(gen_spec_count);

#line 659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_kind[gen_spec_count] = 2;

#line 659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_decl[gen_spec_count] = decl;

#line 659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_type[gen_spec_count] = actual;

#line 659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_name[gen_spec_count] = name;

#line 659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_count = (gen_spec_count+1);

#line 660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = actual;

#line 660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((a!=0))
#line 660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(a);

#line 660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}
}

#line 667 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_collect_type(int ty)
#line 667 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_GENERIC))
#line 664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_add_struct_spec(ty);
}
else
#line 666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_TUPLE))
#line 665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_add_tuple_type(ty);
}
else
#line 666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[ty]==TY_PTR)||(node_kind[ty]==TY_ARRAY))||(node_kind[ty]==TY_DYN_ARRAY)))
#line 666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(node_a[ty]);
}
else
#line 666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 689 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_collect_expr(int id)
#line 689 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 669 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((id==0))
#line 669 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 669 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 670 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int k = node_kind[id];

#line 671 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_VARIANT))
#line 671 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 671 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 671 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((aa!=0))
#line 671 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 671 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(aa);

#line 671 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];
}

#line 671 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 671 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_CALL))
#line 684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 673 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = tc_find_function_ctx(node_value[id], node_scope[id]);

#line 681 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((f!=0)&&(node_kind[f]==N_GENERIC_FUNC)))
#line 681 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 675 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 675 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int actual = 0;

#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((aa!=0))
#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int q = tc_emit_arg_type(aa);

#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((q==0))
#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
q = tc_result_type;

#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((q==0))
#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
q = tc_type_node_from_summary(tc_kind, tc_name, tc_elem_kind, tc_elem_name);
else
#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((q!=0))
#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
q = gen_substitute_type(q);
else
#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((actual==0))
#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
actual = q;
else
#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
actual = ast_link(actual, q);

#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(q);

#line 676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];
}

#line 677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int unresolved = 0;

#line 678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int check_actual = actual;

#line 679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((check_actual!=0))
#line 679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_type_has_param(check_actual)==1))
#line 679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
unresolved = 1;
else
#line 679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
check_actual = node_next[check_actual];
}

#line 680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((unresolved==0))
#line 680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_add_fun_spec(f, actual);
else
#line 680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 681 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aar = node_a[id];

#line 682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((aar!=0))
#line 682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(aar);

#line 682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aar = node_next[aar];
}

#line 683 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_INDIRECT_CALL))
#line 685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_a[id]);

#line 685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_b[id];

#line 685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((aa!=0))
#line 685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(aa);

#line 685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];
}

#line 685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_BINOP))
#line 686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_a[id]);

#line 686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_b[id]);

#line 686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_TUPLE))
#line 687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[id];

#line 687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(item);

#line 687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((k==N_FIELD_ACCESS)||(k==N_INDEX))||(k==N_DEREF))||(k==N_ADDRESS)))
#line 688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_a[id]);

#line 688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_b[id]);

#line 688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 705 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_collect_stmt(int id)
#line 705 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 691 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((id==0))
#line 691 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 691 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 692 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int k = node_kind[id];

#line 693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_DEFER))
#line 693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_a[id]);

#line 693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_TUPLE_BIND))
#line 694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(node_b[id]);

#line 694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_c[id]);

#line 694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_TUPLE))
#line 695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_a[id]);

#line 695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_MATCH))
#line 696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_a[id]);

#line 696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ma = node_b[id];

#line 696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((ma!=0))
#line 696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_stmt(node_b[ma]);

#line 696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ma = node_next[ma];
}

#line 696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 697 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((k==N_LET)||(k==N_CONST)))
#line 697 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 697 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(node_b[id]);

#line 697 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_c[id]);

#line 697 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 697 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_GLOBAL))
#line 698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(node_b[id]);

#line 698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_c[id]);

#line 698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((k==N_ASSIGN)||(k==N_COMPOUND_ASSIGN)))
#line 699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_a[id]);

#line 699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_b[id]);

#line 699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 700 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((k==N_PRINT)||(k==N_EXPR))||(k==N_RETURN)))
#line 700 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 700 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_a[id]);

#line 700 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 700 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_BLOCK))
#line 701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int x = node_a[id];

#line 701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((x!=0))
#line 701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_stmt(x);

#line 701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
x = node_next[x];
}

#line 701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_IF))
#line 702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_a[id]);

#line 702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_stmt(node_b[id]);

#line 702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_stmt(node_c[id]);

#line 702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_WHILE))
#line 703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_a[id]);

#line 703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_stmt(node_b[id]);

#line 703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_FOR))
#line 704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_stmt(node_a[id]);

#line 704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_expr(node_b[id]);

#line 704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_stmt(node_c[id]);

#line 704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_stmt(node_value[id]);

#line 704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_alignment(int alignment)
#line 713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((alignment>0))
#line 712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 708 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1020));

#line 709 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_INT, alignment);

#line 711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
}
else
#line 712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_type(int kind, int child, int size)
#line 761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_INT))
#line 716 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 1);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_BOOL))
#line 717 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 2);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_STRING))
#line 718 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 3);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_CHAR))
#line 719 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 17);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_FLOAT))
#line 720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 18);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_DOUBLE))
#line 721 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 15);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_LONG))
#line 722 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 19);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_LLONG))
#line 723 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 20);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_U8))
#line 724 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 21);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_U16))
#line 725 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 22);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_U32))
#line 726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 23);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_U64))
#line 727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 24);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_I8))
#line 728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 25);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_I16))
#line 729 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 26);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_I32))
#line 730 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 27);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_I64))
#line 731 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 28);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_USIZE))
#line 732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 29);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_VOID))
#line 733 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 4);
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_PTR))
#line 737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 735 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[node_a[child]], node_a[child], 0);

#line 736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 1);
}
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_ARRAY))
#line 742 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[child], node_a[child], node_value[child]);

#line 739 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 2);

#line 740 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_INT, size);

#line 741 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 3);
}
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_DYN_ARRAY))
#line 745 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1003));

#line 744 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 18);
}
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_NAMED))
#line 748 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 746 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_value[child]));

#line 747 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 18);
}
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_GENERIC))
#line 751 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 749 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_mangled_type_symbol(child));

#line 750 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 18);
}
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_TUPLE))
#line 755 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 752 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 12);

#line 753 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_mangled_type_symbol(child));

#line 754 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 18);
}
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_PARAM))
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 756 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int b = gen_bind_find(node_value[child]);

#line 758 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((b!=0))
#line 757 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[b], b, node_value[b]);
else
#line 758 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_value[child]));

#line 759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 18);
}
else
#line 760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_scalar_kind(int arg)
#line 778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 776 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((arg!=0))
#line 776 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int typed = tc_emit_arg_type(arg);

#line 769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((typed!=0))
#line 769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 767 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int resolved = gen_substitute_type(typed);

#line 768 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((resolved!=0)&&(node_kind[resolved]!=TY_PARAM)))
#line 768 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_kind[resolved];
else
#line 768 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[arg]==N_INT))
#line 770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
else
#line 770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 771 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[arg]==N_BOOL))
#line 771 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_BOOL;
else
#line 771 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[arg]==N_CHAR))
#line 772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_CHAR;
else
#line 772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 773 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[arg]==N_FLOAT))
#line 773 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_DOUBLE;
else
#line 773 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 774 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[arg]==N_STRING))
#line 774 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_STRING;
else
#line 774 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[arg]==N_VAR))
#line 775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return sym_type[node_value[arg]];
else
#line 775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 776 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 777 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_expr_kind(arg);
}

#line 790 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_scalar_name(int arg)
#line 790 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((arg!=0))
#line 788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 782 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int typed = tc_emit_arg_type(arg);

#line 786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((typed!=0))
#line 786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 784 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int resolved = gen_substitute_type(typed);

#line 785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((resolved!=0)&&(node_kind[resolved]==TY_NAMED)))
#line 785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_value[resolved];
else
#line 785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[arg]==N_VAR))
#line 787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return sym_elem_name[node_value[arg]];
else
#line 787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 789 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_array_elem_kind(int arg)
#line 814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 812 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((arg!=0)&&(node_kind[arg]==N_VAR)))
#line 812 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = node_value[arg];

#line 795 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int formal_type = gen_active_param_type(name);

#line 799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((formal_type!=0))
#line 799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 797 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int resolved_type = gen_substitute_type(formal_type);

#line 798 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((resolved_type!=0)&&(node_kind[resolved_type]==TY_DYN_ARRAY))&&(node_a[resolved_type]!=0)))
#line 798 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_kind[node_a[resolved_type]];
else
#line 798 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 800 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int kind = sym_elem_kind[name];

#line 801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int elem_name = sym_elem_name[name];

#line 802 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int param_name = elem_name;

#line 803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((kind==TY_PARAM)&&(elem_name!=0))&&(node_kind[elem_name]==TY_PARAM)))
#line 803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
param_name = node_value[elem_name];
else
#line 803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_PARAM))
#line 810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 805 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bound = gen_bind_find(param_name);

#line 809 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bound!=0))
#line 809 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int resolved = gen_substitute_type(bound);

#line 808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((resolved!=0))
#line 808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_kind[resolved];
else
#line 808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 809 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return kind;
}
else
#line 812 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
}

#line 839 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_array_elem_name(int arg)
#line 839 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((arg!=0)&&(node_kind[arg]==N_VAR)))
#line 837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 818 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = node_value[arg];

#line 819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int formal_type = gen_active_param_type(name);

#line 823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((formal_type!=0))
#line 823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 821 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int resolved_type = gen_substitute_type(formal_type);

#line 822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((resolved_type!=0)&&(node_kind[resolved_type]==TY_DYN_ARRAY))&&(node_a[resolved_type]!=0))&&(node_kind[node_a[resolved_type]]==TY_NAMED)))
#line 822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_value[node_a[resolved_type]];
else
#line 822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 824 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int kind = sym_elem_kind[name];

#line 825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int elem_name = sym_elem_name[name];

#line 826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int param_name = elem_name;

#line 827 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((kind==TY_PARAM)&&(elem_name!=0))&&(node_kind[elem_name]==TY_PARAM)))
#line 827 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
param_name = node_value[elem_name];
else
#line 827 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_PARAM))
#line 835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bound = gen_bind_find(param_name);

#line 833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bound!=0))
#line 833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 831 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int resolved = gen_substitute_type(bound);

#line 832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((resolved!=0)&&(node_kind[resolved]==TY_NAMED)))
#line 832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_value[resolved];
else
#line 832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 834 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return elem_name;
}
else
#line 837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 838 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 862 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_array_elem_type(int kind, int name)
#line 862 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_INT))
#line 842 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 1);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_BOOL))
#line 843 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 2);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_CHAR))
#line 844 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 17);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_FLOAT))
#line 845 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 18);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_DOUBLE))
#line 846 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 15);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_LONG))
#line 847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 19);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_LLONG))
#line 848 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 20);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_U8))
#line 849 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 21);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_U16))
#line 850 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 22);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_U32))
#line 851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 23);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_U64))
#line 852 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 24);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_I8))
#line 853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 25);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_I16))
#line 854 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 26);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_I32))
#line 855 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 27);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_I64))
#line 856 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 28);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_USIZE))
#line 857 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 29);
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_STRING))
#line 858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 17);

#line 858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 1);
}
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_NAMED))
#line 859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(name));
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_PTR))
#line 860 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 860 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 1);

#line 860 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 1);
}
else
#line 861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 1);
}

#line 869 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_array_sizeof(int kind, int name)
#line 869 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1011));

#line 866 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 867 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_array_elem_type(kind, name);

#line 868 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
}

#line 888 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_array_value_ptr(int kind, int name, int value)
#line 888 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 872 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 10);

#line 873 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 874 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_array_elem_type(kind, name);

#line 878 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_NAMED))
#line 878 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 876 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 2);

#line 877 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 3);
}
else
#line 878 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 879 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 880 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 24);

#line 885 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_FLOAT))
#line 885 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 882 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 883 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 18);

#line 884 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
}
else
#line 885 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 886 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(value);

#line 887 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 25);
}

#line 897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_array_sizeof_node(int ty)
#line 897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 891 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1011));

#line 892 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 895 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ty!=0)&&(node_kind[ty]==TY_GENERIC)))
#line 893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_mangled_type_symbol(ty));
else
#line 895 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty!=0))
#line 894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_array_elem_type(node_kind[ty], node_value[ty]);
else
#line 895 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_array_elem_type(TY_INT, 0);

#line 896 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
}

#line 907 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_memory_sizeof(int arg)
#line 907 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ty = tc_emit_arg_type(arg);

#line 901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty!=0))
#line 901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = gen_substitute_type(ty);
else
#line 901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 906 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((ty!=0)&&(node_kind[ty]==TY_PTR))&&(node_a[ty]!=0)))
#line 905 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int elem = gen_substitute_type(node_a[ty]);

#line 904 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_array_sizeof_node(elem);
}
else
#line 906 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty!=0))
#line 905 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_array_sizeof_node(ty);
else
#line 906 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_array_sizeof_node(0);
}

#line 1000 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_memory_builtin(int id)
#line 1000 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 910 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int call_name = node_value[id];

#line 911 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int btag = bi_tag(call_name);

#line 912 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = node_a[id];

#line 999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((btag==BI_TC_MEM_ALLOC)||(btag==BI_TC_MEM_ALLOC_ALIGNED)))
#line 951 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 914 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int alignment_arg = 0;

#line 915 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int witness = node_next[a];

#line 916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_MEM_ALLOC_ALIGNED))
#line 916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
alignment_arg = witness;

#line 916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
witness = node_next[witness];
}
else
#line 916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int elem_kind = gen_scalar_kind(witness);

#line 918 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int elem_name = gen_scalar_name(witness);

#line 919 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_witness_ty = 0;

#line 923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((elem_kind==TY_GENERIC))
#line 923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 921 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int wty = tc_emit_arg_type(witness);

#line 922 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((wty!=0))
#line 922 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_witness_ty = gen_substitute_type(wty);
else
#line 922 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 924 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 925 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 926 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 4);

#line 927 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 928 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 929 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(witness);

#line 930 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 931 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);

#line 932 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 934 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_witness_ty!=0))
#line 933 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_mangled_type_symbol(gen_witness_ty));
else
#line 934 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_array_elem_type(elem_kind, elem_name);

#line 935 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 1);

#line 936 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_MEM_ALLOC_ALIGNED))
#line 937 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1019));
else
#line 938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1016));

#line 939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 940 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(a);

#line 941 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);

#line 942 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_MEM_ALLOC_ALIGNED))
#line 942 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 942 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(alignment_arg);

#line 942 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
}
else
#line 942 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 948 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_witness_ty!=0))
#line 948 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 944 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1011));

#line 945 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 946 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_mangled_type_symbol(gen_witness_ty));

#line 947 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
}
else
#line 948 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_array_sizeof(elem_kind, elem_name);

#line 949 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 950 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);
}
else
#line 999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_MEM_RESIZE))
#line 984 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 952 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int old_count = node_next[a];

#line 953 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int new_count = node_next[old_count];

#line 954 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int zero = node_next[new_count];

#line 955 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ptr_ty = tc_emit_arg_type(a);

#line 956 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ptr_ty!=0))
#line 956 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ptr_ty = gen_substitute_type(ptr_ty);
else
#line 956 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 957 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ptr_ty==0))
#line 957 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ptr_ty = ast_node(TY_PTR, ast_node(TY_INT, 0, 0, 0, 0, 0), 0, 0, 0, 0);
else
#line 957 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 958 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 959 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 960 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 4);

#line 961 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 962 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 963 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(zero);

#line 964 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 965 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);

#line 966 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 967 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[ptr_ty], ptr_ty, 0);

#line 968 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 969 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1017));

#line 970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 971 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 972 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 4);

#line 973 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 1);

#line 974 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 975 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(a);

#line 976 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);

#line 977 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(old_count);

#line 978 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);

#line 979 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(new_count);

#line 980 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);

#line 981 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_memory_sizeof(a);

#line 982 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 983 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);
}
else
#line 999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_MEM_FREE))
#line 993 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 985 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1018));

#line 986 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 987 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 988 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 4);

#line 989 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 1);

#line 990 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 991 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(a);

#line 992 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);
}
else
#line 999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 994 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(call_name));

#line 995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 996 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arg = a;

#line 997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((arg!=0))
#line 997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(arg);

#line 997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[arg]!=0))
#line 997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
else
#line 997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arg = node_next[arg];
}

#line 998 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);
}
}

#line 1041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_call_name(int id)
#line 1041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1003 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = tc_find_function_ctx(node_value[id], node_scope[id]);

#line 1004 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((f==0))
#line 1004 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return sym_c_symbol(node_value[id]);
else
#line 1004 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1005 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[f]!=N_GENERIC_FUNC))
#line 1005 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return sym_c_symbol(node_value[f]);
else
#line 1005 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1006 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int actual = 0;

#line 1006 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = node_a[id];

#line 1014 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((a!=0))
#line 1014 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1008 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int q = 0;

#line 1009 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
q = tc_emit_arg_type(a);

#line 1010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((q==0))
#line 1010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(a);

#line 1010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
q = tc_result_type;

#line 1010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((q==0))
#line 1010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
q = tc_type_node_from_summary(tc_kind, tc_name, tc_elem_kind, tc_elem_name);
else
#line 1010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 1010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1011 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((q!=0))
#line 1011 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
q = gen_substitute_type(q);
else
#line 1011 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1012 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((actual==0))
#line 1012 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
actual = q;
else
#line 1012 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
actual = ast_link(actual, q);

#line 1013 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}

#line 1015 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int saved_count = gen_bind_count;

#line 1016 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_bind((saved_count+saved_count));

#line 1017 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int save_i = 0;

#line 1022 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((save_i<saved_count))
#line 1022 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1019 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[(saved_count+save_i)] = gen_bind_name[save_i];

#line 1020 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[(saved_count+save_i)] = gen_bind_type[save_i];

#line 1021 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
save_i = (save_i+1);
}

#line 1023 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_decl(f, actual);

#line 1024 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int typeargs = 0;

#line 1024 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tp = node_aux[f];

#line 1031 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((tp!=0))
#line 1031 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1026 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bt = gen_bind_find(node_a[tp]);

#line 1027 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bt==0))
#line 1027 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bt = ast_node(TY_PARAM, 0, 0, 0, node_a[tp], 0);
else
#line 1027 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1028 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int cq = gen_substitute_type(bt);

#line 1029 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((typeargs==0))
#line 1029 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
typeargs = cq;
else
#line 1029 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
typeargs = ast_link(typeargs, cq);

#line 1030 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tp = node_next[tp];
}

#line 1032 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_clear();

#line 1033 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int restore_i = 0;

#line 1038 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((restore_i<saved_count))
#line 1038 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1035 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[restore_i] = gen_bind_name[(saved_count+restore_i)];

#line 1036 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[restore_i] = gen_bind_type[(saved_count+restore_i)];

#line 1037 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
restore_i = (restore_i+1);
}

#line 1039 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_count = saved_count;

#line 1040 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_mangled_function_symbol(node_value[f], typeargs);
}

#line 1088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_variant_expr(int id)
#line 1088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1044 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_find_enum_variant(node_value[id]);

#line 1045 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int enum_name = tc_variant_enum;

#line 1046 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((enum_name==0)&&(node_aux[id]!=0))&&(node_kind[node_aux[id]]==TY_NAMED)))
#line 1046 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
enum_name = node_value[node_aux[id]];
else
#line 1046 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1047 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int member = tc_variant_member;

#line 1048 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((member==0))
#line 1048 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1048 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_INT, 0);

#line 1048 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 1048 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1049 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int enum_decl = tc_find_enum(enum_name);

#line 1050 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tagged = 0;

#line 1057 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((enum_decl!=0))
#line 1057 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1052 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int probe = node_a[enum_decl];

#line 1056 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((probe!=0))
#line 1056 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1054 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_b[probe]!=0))
#line 1054 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tagged = 1;
else
#line 1054 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1055 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
probe = node_next[probe];
}
}
else
#line 1057 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1058 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tagged==0))
#line 1058 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1058 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(sym_qualified(enum_name, node_a[member])));

#line 1058 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 1058 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1059 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(enum_name));

#line 1061 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1062 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 24);

#line 1063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 17);

#line 1064 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_tag_id());

#line 1065 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1066 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(sym_qualified(enum_name, node_a[member])));

#line 1067 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int payload = node_b[member];

#line 1086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((payload!=0))
#line 1086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1069 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);

#line 1070 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 17);

#line 1071 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_a[member]));

#line 1072 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1073 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 24);

#line 1074 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arg = node_a[id];

#line 1075 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = payload;

#line 1084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((field!=0)&&(arg!=0)))
#line 1084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1077 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 17);

#line 1078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_a[field]));

#line 1079 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1080 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(arg);

#line 1081 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_next[field]!=0)&&(node_next[arg]!=0)))
#line 1081 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
else
#line 1081 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1082 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
field = node_next[field];

#line 1083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arg = node_next[arg];
}

#line 1085 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 25);
}
else
#line 1086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1087 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 25);
}

#line 1179 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_expr(int id)
#line 1179 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1091 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int k = node_kind[id];

#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_INT))
#line 1096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1095 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_aux[id]!=0))
#line 1095 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_value[id]<0))
#line 1094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_RAW_U64, node_aux[id]);
else
#line 1094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_RAW, node_aux[id]);
}
else
#line 1095 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_INT, node_value[id]);
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_BOOL))
#line 1097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_INT, node_value[id]);
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_FLOAT))
#line 1101 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1099 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, node_value[id]);

#line 1100 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_aux[id]==TY_FLOAT))
#line 1100 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1021));
else
#line 1100 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_STRING))
#line 1102 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_STRING, node_value[id]);
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_CHAR))
#line 1103 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_INT, node_value[id]);
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_VARIANT))
#line 1104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_variant_expr(id);
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_TUPLE))
#line 1128 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tuple_ty = node_aux[id];

#line 1107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tuple_ty==0))
#line 1107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(id);

#line 1107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tuple_ty = tc_result_type;
}
else
#line 1107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1127 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tuple_ty==0))
#line 1108 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1108 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 24);

#line 1108 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 25);
}
else
#line 1127 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1111 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 12);

#line 1112 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_mangled_type_symbol(tuple_ty));

#line 1113 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1114 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 24);

#line 1115 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[id];

#line 1116 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int index = 0;

#line 1125 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 1125 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 17);

#line 1119 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_tuple_field_name(index));

#line 1120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1121 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(item);

#line 1122 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[item]!=0))
#line 1122 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
else
#line 1122 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1123 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];

#line 1124 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
index = (index+1);
}

#line 1126 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 25);
}
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_NULL))
#line 1129 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1129 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_INT, 0);
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_VAR))
#line 1153 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_aux[id]!=0)&&(node_kind[node_aux[id]]==TY_NAMED)))
#line 1152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1132 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int enum_name = node_value[node_aux[id]];

#line 1133 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int enum_decl = tc_find_enum(enum_name);

#line 1134 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int member = tc_match_variant_member(enum_decl, node_value[id]);

#line 1151 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((member!=0))
#line 1151 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1136 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tagged = 0;

#line 1137 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int probe = node_a[enum_decl];

#line 1138 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((probe!=0))
#line 1138 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1138 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_b[probe]!=0))
#line 1138 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tagged = 1;
else
#line 1138 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1138 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
probe = node_next[probe];
}

#line 1150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tagged==0))
#line 1139 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(sym_qualified(enum_name, node_a[member])));
else
#line 1150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1142 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(enum_name));

#line 1143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 24);

#line 1145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 17);

#line 1146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_tag_id());

#line 1147 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1148 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(sym_qualified(enum_name, node_a[member])));

#line 1149 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 25);
}
}
else
#line 1151 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_value[id]));
}
else
#line 1152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_value[id]));
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_BINOP))
#line 1160 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_c[id]==TY_FLOAT))
#line 1155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 18);

#line 1155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);
}
else
#line 1155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_value[id]==OP_CONCAT))
#line 1156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1002));

#line 1156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 1156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);

#line 1156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);

#line 1156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_b[id]);

#line 1156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);
}
else
#line 1158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_value[id]==OP_SUB)&&(gen_expr_kind(node_a[id])==TY_PTR))&&(gen_expr_kind(node_b[id])==TY_PTR)))
#line 1157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 1);

#line 1157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);

#line 1157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_OP, node_value[id]);

#line 1157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_b[id]);

#line 1157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
}
else
#line 1158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);

#line 1158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_OP, node_value[id]);

#line 1158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_b[id]);

#line 1158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
}

#line 1159 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_c[id]==TY_FLOAT))
#line 1159 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
else
#line 1159 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_CALL))
#line 1165 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1161 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int call_name = node_value[id];

#line 1162 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int btag = bi_tag(call_name);

#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((btag==BI_TC_MEM_ALLOC)||(btag==BI_TC_MEM_ALLOC_ALIGNED))||(btag==BI_TC_MEM_RESIZE))||(btag==BI_TC_MEM_FREE)))
#line 1163 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_memory_builtin(id);
else
#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_call_name(id));

#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arg = node_a[id];

#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((arg!=0))
#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(arg);

#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[arg]!=0))
#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
else
#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arg = node_next[arg];
}

#line 1164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);
}
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_INDIRECT_CALL))
#line 1169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1166 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1166 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);

#line 1166 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1166 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 1167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arg = node_b[id];

#line 1167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((arg!=0))
#line 1167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(arg);

#line 1167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[arg]!=0))
#line 1167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
else
#line 1167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arg = node_next[arg];
}

#line 1168 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_DEREF))
#line 1169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 9);

#line 1169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_ADDRESS))
#line 1170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 10);

#line 1170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_INDEX))
#line 1173 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1172 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);

#line 1172 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 2);

#line 1172 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_b[id]);

#line 1172 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 3);
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_FIELD_ACCESS))
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1175 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);

#line 1176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_expr_kind(node_a[id])==TY_PTR))
#line 1176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 27);
else
#line 1176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 17);

#line 1177 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, node_value[id]);
}
else
#line 1178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 1228 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_expr_kind(int id)
#line 1228 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int k = node_kind[id];

#line 1183 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((k==N_INT)||(k==N_BOOL)))
#line 1183 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
else
#line 1183 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1184 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_TUPLE))
#line 1184 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_TUPLE;
else
#line 1184 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_VARIANT))
#line 1185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_aux[id]!=0))
#line 1185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_kind[node_aux[id]];
else
#line 1185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_NAMED;
}
else
#line 1185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_CHAR))
#line 1186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_CHAR;
else
#line 1186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_FLOAT))
#line 1187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_aux[id]==TY_FLOAT))
#line 1187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_FLOAT;
else
#line 1187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_DOUBLE;
}
else
#line 1187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1188 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_STRING))
#line 1188 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_STRING;
else
#line 1188 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1189 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_NULL))
#line 1189 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_PTR;
else
#line 1189 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_VAR))
#line 1203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1191 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int formal_type = gen_active_param_type(node_value[id]);

#line 1195 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((formal_type!=0))
#line 1195 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1193 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int resolved_type = gen_substitute_type(formal_type);

#line 1194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((resolved_type!=0))
#line 1194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_kind[resolved_type];
else
#line 1194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 1195 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1196 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int vt = sym_type[node_value[id]];

#line 1201 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((vt>99))
#line 1201 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1198 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = sym_elem_kind[node_value[id]];

#line 1199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = sym_elem_name[node_value[id]];

#line 1200 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(vt-100);
}
else
#line 1201 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1202 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return vt;
}
else
#line 1203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1209 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_INDEX))
#line 1209 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1205 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bt = gen_expr_kind(node_a[id]);

#line 1206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bt==TY_STRING))
#line 1206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_CHAR;
else
#line 1206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1207 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bt==TY_PTR))
#line 1207 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
else
#line 1207 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1208 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
}
else
#line 1209 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1210 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_DEREF))
#line 1210 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
else
#line 1210 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1211 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_ADDRESS))
#line 1211 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_PTR;
else
#line 1211 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_FIELD_ACCESS))
#line 1212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field_ty = tc_emit_field_type(id);

#line 1212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((field_ty!=0))
#line 1212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_kind[field_ty];
else
#line 1212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
}
else
#line 1212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1213 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((k==N_CALL)||(k==N_INDIRECT_CALL)))
#line 1213 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_expr_kind_for_emit(id);
else
#line 1213 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_BINOP))
#line 1226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_value[id]==OP_CONCAT))
#line 1215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_STRING;
else
#line 1215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1216 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((((node_value[id]==OP_EQ)||(node_value[id]==OP_NEQ))||(node_value[id]==OP_LT))||(node_value[id]==OP_GT))||(node_value[id]==OP_AND))||(node_value[id]==OP_OR)))
#line 1216 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_BOOL;
else
#line 1216 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1217 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_value[id]==OP_SUB)&&(gen_expr_kind(node_a[id])==TY_PTR))&&(gen_expr_kind(node_b[id])==TY_PTR)))
#line 1217 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
else
#line 1217 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ak = gen_expr_kind(node_a[id]);

#line 1218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bk = gen_expr_kind(node_b[id]);

#line 1219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((node_value[id]==OP_BITAND)||(node_value[id]==OP_BITOR))||(node_value[id]==OP_BITXOR))||(node_value[id]==OP_SHL))||(node_value[id]==OP_SHR)))
#line 1219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ak==TY_LLONG)||(bk==TY_LLONG)))
#line 1219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_LLONG;
else
#line 1219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ak==TY_LONG)||(bk==TY_LONG)))
#line 1219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_LONG;
else
#line 1219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
}
else
#line 1219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ak==TY_DOUBLE)||(bk==TY_DOUBLE)))
#line 1220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_DOUBLE;
else
#line 1220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1221 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ak==TY_FLOAT)||(bk==TY_FLOAT)))
#line 1221 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_FLOAT;
else
#line 1221 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1222 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ak==TY_LLONG)||(bk==TY_LLONG)))
#line 1222 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_LLONG;
else
#line 1222 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ak==TY_LONG)||(bk==TY_LONG)))
#line 1223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_LONG;
else
#line 1223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1224 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ak==TY_PTR))
#line 1224 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_PTR;
else
#line 1224 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1225 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
}
else
#line 1226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1227 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
}

#line 1236 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_initializer(int ty, int expr)
#line 1236 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1231 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int st = gen_substitute_type(ty);

#line 1235 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((node_kind[st]==TY_NAMED)||(node_kind[st]==TY_GENERIC))||(node_kind[st]==TY_ARRAY))&&(node_kind[expr]==N_INT))&&(node_value[expr]==0)))
#line 1232 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 19);
else
#line 1235 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[st]==TY_FLOAT)&&(gen_expr_kind(expr)!=TY_FLOAT)))
#line 1235 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 18);

#line 1234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(expr);

#line 1234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
}
else
#line 1235 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(expr);
}

#line 1244 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_assignment(int lhs, int rhs)
#line 1244 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1239 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(lhs);

#line 1240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((gen_expr_kind(lhs)==TY_FLOAT)&&(gen_expr_kind(rhs)!=TY_FLOAT)))
#line 1243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 18);

#line 1242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(rhs);

#line 1242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
}
else
#line 1243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(rhs);
}

#line 1258 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int compound_c_operator(int op)
#line 1258 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((op==OP_ADD))
#line 1247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 19;
else
#line 1247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1248 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((op==OP_SUB))
#line 1248 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 20;
else
#line 1248 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((op==OP_MUL))
#line 1249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 21;
else
#line 1249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1250 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((op==OP_DIV))
#line 1250 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 22;
else
#line 1250 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1251 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((op==OP_MOD))
#line 1251 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 23;
else
#line 1251 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((op==OP_BITAND))
#line 1252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 24;
else
#line 1252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((op==OP_BITOR))
#line 1253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 25;
else
#line 1253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1254 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((op==OP_BITXOR))
#line 1254 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 26;
else
#line 1254 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1255 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((op==OP_SHL))
#line 1255 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 27;
else
#line 1255 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((op==OP_SHR))
#line 1256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 28;
else
#line 1256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1257 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 19;
}

#line 1266 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_compound_assignment(int lhs, int op, int rhs)
#line 1266 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1261 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(lhs);

#line 1262 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_OP, compound_c_operator(op));

#line 1265 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((gen_expr_kind(lhs)==TY_FLOAT)&&(gen_expr_kind(rhs)!=TY_FLOAT)))
#line 1265 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 18);

#line 1264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(rhs);

#line 1264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
}
else
#line 1265 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(rhs);
}

#line 1280 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_for_clause(int id)
#line 1280 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1269 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((id==0))
#line 1269 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1269 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_LET))
#line 1275 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1271 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[node_b[id]], node_b[id], node_value[node_b[id]]);

#line 1272 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, node_a[id]);

#line 1273 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1274 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_initializer(node_b[id], node_c[id]);
}
else
#line 1279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_ASSIGN))
#line 1277 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_assignment(node_a[id], node_b[id]);
}
else
#line 1279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_COMPOUND_ASSIGN))
#line 1279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1278 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_compound_assignment(node_a[id], node_value[id], node_b[id]);
}
else
#line 1279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_EXPR))
#line 1279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);
else
#line 1279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 1297 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_fun_decl(int ty, int name)
#line 1297 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1283 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ret = node_b[ty];

#line 1284 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[ret], ret, node_value[ret]);

#line 1285 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1286 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 9);

#line 1287 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(name));

#line 1288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1289 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 1290 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_a[ty];

#line 1295 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 1295 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[p], p, node_value[p]);

#line 1293 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[p]!=0))
#line 1293 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
else
#line 1293 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1294 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 1296 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);
}

#line 1309 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_decl(int ty, int name)
#line 1309 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1308 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_FUN))
#line 1300 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_fun_decl(ty, name);
else
#line 1308 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_ARRAY))
#line 1305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1302 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int inner = node_a[ty];

#line 1303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[inner], inner, node_value[inner]);

#line 1304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(name));

#line 1304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 2);

#line 1304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_INT, node_value[ty]);

#line 1304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 3);
}
else
#line 1308 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[ty], ty, node_value[ty]);

#line 1307 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(name));
}
}

#line 1470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_stmt(int id)
#line 1470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_pos = node_pos[id];

#line 1313 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_epoch = (gen_source_epoch+1);

#line 1314 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int k = node_kind[id];

#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_GLOBAL))
#line 1322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_alignment(node_aux[id]);

#line 1317 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_decl(node_b[id], node_a[id]);

#line 1318 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_initializer(node_b[id], node_c[id]);

#line 1320 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1321 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_CONST))
#line 1330 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1323 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_alignment(node_aux[id]);

#line 1324 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 16);

#line 1325 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_decl(node_b[id], node_a[id]);

#line 1326 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1327 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_initializer(node_b[id], node_c[id]);

#line 1328 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1329 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_LET))
#line 1337 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_alignment(node_aux[id]);

#line 1332 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_decl(node_b[id], node_a[id]);

#line 1333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1334 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_initializer(node_b[id], node_c[id]);

#line 1335 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1336 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_ASSIGN))
#line 1341 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1338 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_assignment(node_a[id], node_b[id]);

#line 1339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1340 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_COMPOUND_ASSIGN))
#line 1345 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_compound_assignment(node_a[id], node_value[id], node_b[id]);

#line 1343 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1344 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_PRINT))
#line 1365 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1346 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, (0-1001));

#line 1347 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pk = gen_expr_kind(node_a[id]);

#line 1360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((pk==TY_STRING))
#line 1348 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 16);
else
#line 1360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((pk==TY_CHAR))
#line 1349 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 20);
else
#line 1360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((pk==TY_FLOAT)||(pk==TY_DOUBLE)))
#line 1350 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 21);
else
#line 1360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((pk==TY_LONG))
#line 1351 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 28);
else
#line 1360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((pk==TY_LLONG))
#line 1352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 29);
else
#line 1360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((pk==TY_PTR))
#line 1359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 26);

#line 1355 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1356 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 4);

#line 1357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 1);

#line 1358 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);
}
else
#line 1360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 15);

#line 1361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);

#line 1362 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 1363 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1364 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_DEFER))
#line 1367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1366 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_defer_push(node_a[id]);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_TUPLE_BIND))
#line 1394 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int binding = node_a[id];

#line 1369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int index = 0;

#line 1370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tuple_ty = node_b[id];

#line 1371 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int rhs_temp = gen_match_temp_symbol();

#line 1372 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[tuple_ty], tuple_ty, 0);

#line 1373 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, rhs_temp);

#line 1374 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_c[id]);

#line 1376 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1377 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1393 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((binding!=0))
#line 1393 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1379 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int elem_ty = node_a[tuple_ty];

#line 1380 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 1381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((i<index)&&(elem_ty!=0)))
#line 1381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
elem_ty = node_next[elem_ty];

#line 1381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 1383 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((elem_ty!=0))
#line 1382 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[elem_ty], elem_ty, 0);
else
#line 1383 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 1);

#line 1384 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_value[binding]));

#line 1385 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1386 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, rhs_temp);

#line 1387 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 17);

#line 1388 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_tuple_field_name(index));

#line 1389 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1390 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1391 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
binding = node_next[binding];

#line 1392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
index = (index+1);
}
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_MATCH))
#line 1396 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_match_stmt(id);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_EXPR))
#line 1400 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);

#line 1398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_RETURN))
#line 1406 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_all_defers();

#line 1402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 5);

#line 1403 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[id]!=0))
#line 1403 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);
else
#line 1403 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1404 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1405 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_BREAK))
#line 1411 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1407 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((emit_loop_depth>0))
#line 1407 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_defer_from(emit_loop_base[(emit_loop_depth-1)]);
else
#line 1407 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1408 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 9);

#line 1409 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1410 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_CONTINUE))
#line 1417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((emit_loop_depth>0))
#line 1412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_defer_from(emit_loop_base[(emit_loop_depth-1)]);
else
#line 1412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1413 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((emit_for_step!=0))
#line 1413 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_stmt(emit_for_step);
else
#line 1413 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 10);

#line 1415 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1416 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_BLOCK))
#line 1431 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1418 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 13);

#line 1419 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_emit_scope(emit_scope_depth);

#line 1420 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_scope_start[emit_scope_depth] = emit_defer_count;

#line 1421 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_scope_depth = (emit_scope_depth+1);

#line 1422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[id];

#line 1426 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 1426 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_stmt(item);

#line 1425 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 1427 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_scope_depth = (emit_scope_depth-1);

#line 1428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_defer_from(emit_scope_start[emit_scope_depth]);

#line 1429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_defer_count = emit_scope_start[emit_scope_depth];

#line 1430 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 14);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_IF))
#line 1439 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1432 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 6);

#line 1433 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 1434 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);

#line 1435 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 1436 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_stmt(node_b[id]);

#line 1437 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 7);

#line 1438 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_stmt(node_c[id]);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_FOR))
#line 1456 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1440 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 11);

#line 1441 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1442 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_for_clause(node_a[id]);

#line 1443 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1444 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_b[id]);

#line 1445 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1446 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_for_clause(node_value[id]);

#line 1447 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1448 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_emit_loop(emit_loop_depth);

#line 1449 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_loop_base[emit_loop_depth] = emit_defer_count;

#line 1450 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_loop_depth = (emit_loop_depth+1);

#line 1451 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int old_step = emit_for_step;

#line 1452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_for_step = node_value[id];

#line 1453 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_stmt(node_c[id]);

#line 1454 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_for_step = old_step;

#line 1455 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_loop_depth = (emit_loop_depth-1);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_WHILE))
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1457 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 8);

#line 1458 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1459 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(node_a[id]);

#line 1460 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1461 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_emit_loop(emit_loop_depth);

#line 1462 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_loop_base[emit_loop_depth] = emit_defer_count;

#line 1463 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_loop_depth = (emit_loop_depth+1);

#line 1464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int old_step = emit_for_step;

#line 1465 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_for_step = node_aux[id];

#line 1466 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_stmt(node_b[id]);

#line 1467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_for_step = old_step;

#line 1468 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_loop_depth = (emit_loop_depth-1);
}
else
#line 1469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 1488 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_unify_formal(int formal, int actual)
#line 1488 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((formal==0)||(actual==0)))
#line 1473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1478 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[formal]==TY_PARAM))
#line 1478 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1475 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[actual]==TY_PARAM)&&(node_value[formal]==node_value[actual])))
#line 1475 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1475 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1476 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_bind_find(node_value[formal])==0))
#line 1476 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_add(node_value[formal], actual);
else
#line 1476 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1477 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 1478 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[formal]==TY_GENERIC)&&(node_kind[actual]==TY_GENERIC)))
#line 1484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_value[formal]!=node_value[actual]))
#line 1480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int fp = node_a[formal];

#line 1481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ap = node_a[actual];

#line 1482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((fp!=0)&&(ap!=0)))
#line 1482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_unify_formal(fp, ap);

#line 1482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
fp = node_next[fp];

#line 1482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ap = node_next[ap];
}

#line 1483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 1484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[formal]==TY_PTR)&&(node_kind[actual]==TY_PTR)))
#line 1485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_unify_formal(node_a[formal], node_a[actual]);

#line 1485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 1485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[formal]==TY_ARRAY)&&(node_kind[actual]==TY_ARRAY)))
#line 1486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_unify_formal(node_a[formal], node_a[actual]);

#line 1486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 1486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1487 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[formal]==TY_DYN_ARRAY)&&(node_kind[actual]==TY_DYN_ARRAY)))
#line 1487 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1487 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_unify_formal(node_a[formal], node_a[actual]);

#line 1487 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 1487 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 1500 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_bind_decl(int decl, int inst)
#line 1500 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_clear();

#line 1496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[decl]==N_GENERIC_FUNC))
#line 1496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1493 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_c[decl];

#line 1493 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = inst;

#line 1494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((p!=0)&&(a!=0)))
#line 1494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_unify_formal(node_b[p], a);

#line 1494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];

#line 1494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}

#line 1495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 1496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1497 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_c[decl];

#line 1498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = node_a[inst];

#line 1499 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((p!=0)&&(a!=0)))
#line 1499 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1499 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_add(node_a[p], a);

#line 1499 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];

#line 1499 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}
}

#line 1507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_struct_decl_specialized(int decl, int inst, int cname)
#line 1507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1502 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_decl(decl, inst);

#line 1503 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 12);

#line 1503 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, cname);

#line 1503 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 13);

#line 1504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[decl];

#line 1505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((f!=0))
#line 1505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ft = gen_substitute_type(node_b[f]);

#line 1505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_decl(ft, node_a[f]);

#line 1505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];
}

#line 1506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 14);

#line 1506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}

#line 1520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_function_specialized(int decl, int inst, int cname)
#line 1520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1509 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_decl(decl, inst);

#line 1510 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ret = gen_substitute_type(node_b[decl]);

#line 1511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[ret], ret, node_value[ret]);

#line 1511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, cname);

#line 1511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 1512 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_c[decl];

#line 1513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 1513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pt = gen_substitute_type(node_b[p]);

#line 1513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_decl(pt, node_a[p]);

#line 1513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[p]!=0))
#line 1513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
else
#line 1513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 1514 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 1515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int old_active_function = gen_active_function;

#line 1516 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_active_function = decl;

#line 1517 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_stmt(node_a[decl]);

#line 1518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_active_function = old_active_function;

#line 1519 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_clear();
}

#line 1527 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_match_temp_symbol()
#line 1527 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1523 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_start = (source_len+sym_text_len);

#line 1523 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_mangle_len = 0;

#line 1524 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_text("__basalt_match_");

#line 1524 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_append_uint(gen_match_serial);

#line 1525 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_match_serial = (gen_match_serial+1);

#line 1526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_mangle_intern(L_ID);
}

#line 1539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_match_binding(int temp, int variant, int binding, int field)
#line 1539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_decl(node_b[field], node_value[binding]);

#line 1531 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1532 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, temp);

#line 1533 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 17);

#line 1534 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_a[variant]));

#line 1535 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 17);

#line 1536 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_a[field]));

#line 1537 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1538 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}

#line 1584 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_match_stmt(int id)
#line 1584 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1542 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int subject = node_a[id];

#line 1543 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int subject_ty = tc_emit_arg_type(subject);

#line 1544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((subject_ty==0))
#line 1544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(subject);

#line 1544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
subject_ty = tc_result_type;
}
else
#line 1544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1545 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((subject_ty==0))
#line 1545 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1545 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1546 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
subject_ty = gen_substitute_type(subject_ty);

#line 1547 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int enum_decl = tc_match_enum_decl(subject_ty);

#line 1548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((enum_decl==0))
#line 1548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int enum_name = node_value[enum_decl];

#line 1550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tagged = 0;

#line 1551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int probe = node_a[enum_decl];

#line 1552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((probe!=0))
#line 1552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_b[probe]!=0))
#line 1552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tagged = 1;
else
#line 1552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
probe = node_next[probe];
}

#line 1553 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int temp = gen_match_temp_symbol();

#line 1554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[subject_ty], subject_ty, node_value[subject_ty]);

#line 1555 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, temp);

#line 1556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_expr(subject);

#line 1558 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1559 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arm = node_b[id];

#line 1583 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((arm!=0))
#line 1583 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1562 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int variant = tc_match_variant_member(enum_decl, node_value[arm]);

#line 1581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((variant!=0))
#line 1581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1564 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 6);

#line 1565 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 4);

#line 1566 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, temp);

#line 1567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tagged==1))
#line 1567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 17);
else
#line 1567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1568 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tagged==1))
#line 1568 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_tag_id());
else
#line 1568 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1569 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_OP, 5);

#line 1570 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(sym_qualified(enum_name, node_a[variant])));

#line 1571 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 5);

#line 1572 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 13);

#line 1573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = node_b[variant];

#line 1574 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int binding = node_a[arm];

#line 1578 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((field!=0)&&(binding!=0)))
#line 1578 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1576 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_match_binding(temp, variant, binding, field);

#line 1577 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
field = node_next[field];

#line 1577 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
binding = node_next[binding];
}

#line 1579 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_stmt(node_b[arm]);

#line 1580 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 14);
}
else
#line 1581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1582 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arm = node_next[arm];
}
}

#line 1603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_tuple_decl(int ty, int name)
#line 1603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1587 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 12);

#line 1588 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, name);

#line 1589 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 13);

#line 1590 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = node_a[ty];

#line 1591 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int index = 0;

#line 1599 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((field!=0))
#line 1599 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1593 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[field], field, node_value[field]);

#line 1594 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_tuple_field_name(index));

#line 1595 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1596 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
field = node_next[field];

#line 1598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
index = (index+1);
}

#line 1600 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 14);

#line 1601 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}

#line 1621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_struct_decl(int id)
#line 1621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1606 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_pos = node_pos[id];

#line 1607 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_epoch = (gen_source_epoch+1);

#line 1608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 12);

#line 1609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_value[id]));

#line 1610 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 13);

#line 1611 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[id];

#line 1617 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((f!=0))
#line 1617 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_decl(node_b[f], node_a[f]);

#line 1614 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];
}

#line 1618 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 14);

#line 1619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1620 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}

#line 1636 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_emit_complete_struct(int decl)
#line 1636 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((decl==0))
#line 1624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1625 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_struct_state(decl);

#line 1626 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_struct_state[decl]==2))
#line 1626 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1626 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_struct_state[decl]==1))
#line 1627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_struct_state[decl] = 1;

#line 1629 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = node_a[decl];

#line 1633 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((field!=0))
#line 1633 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1631 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_complete_type(node_b[field]);

#line 1632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
field = node_next[field];
}

#line 1634 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_struct_decl(decl);

#line 1635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_struct_state[decl] = 2;
}

#line 1678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_emit_complete_spec(int index)
#line 1678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((index<0))
#line 1639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1640 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_spec_state(index);

#line 1641 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_state[index]==2))
#line 1641 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1641 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1642 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_state[index]==1))
#line 1642 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1642 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1643 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_state[index] = 1;

#line 1644 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int decl = gen_spec_decl[index];

#line 1645 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int inst = gen_spec_type[index];

#line 1646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int saved_count = gen_bind_count;

#line 1647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_bind((saved_count+saved_count));

#line 1648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int save_i = 0;

#line 1653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((save_i<saved_count))
#line 1653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[(saved_count+save_i)] = gen_bind_name[save_i];

#line 1651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[(saved_count+save_i)] = gen_bind_type[save_i];

#line 1652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
save_i = (save_i+1);
}

#line 1654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_decl(decl, inst);

#line 1655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = node_a[decl];

#line 1659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((field!=0))
#line 1659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1657 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_complete_type(gen_substitute_type(node_b[field]));

#line 1658 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
field = node_next[field];
}

#line 1660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_clear();

#line 1661 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int restore_i = 0;

#line 1666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((restore_i<saved_count))
#line 1666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[restore_i] = gen_bind_name[(saved_count+restore_i)];

#line 1664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[restore_i] = gen_bind_type[(saved_count+restore_i)];

#line 1665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
restore_i = (restore_i+1);
}

#line 1667 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_count = saved_count;

#line 1668 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_struct_decl_specialized(decl, inst, gen_spec_name[index]);

#line 1669 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_clear();

#line 1670 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int restore_j = 0;

#line 1675 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((restore_j<saved_count))
#line 1675 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1672 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[restore_j] = gen_bind_name[(saved_count+restore_j)];

#line 1673 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[restore_j] = gen_bind_type[(saved_count+restore_j)];

#line 1674 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
restore_j = (restore_j+1);
}

#line 1676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_count = saved_count;

#line 1677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_state[index] = 2;
}

#line 1695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_emit_complete_type(int ty)
#line 1695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1681 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int q = gen_substitute_type(ty);

#line 1682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((q==0))
#line 1682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1683 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[q]==TY_PTR))
#line 1683 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1683 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[q]==TY_ARRAY))
#line 1684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_complete_type(node_a[q]);

#line 1684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 1684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[q]==TY_DYN_ARRAY))
#line 1685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 1685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1689 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[q]==TY_NAMED))
#line 1689 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_complete_struct(tc_find_struct(node_value[q]));

#line 1688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 1689 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[q]==TY_GENERIC))
#line 1694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1691 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int decl = tc_find_struct(node_value[q]);

#line 1692 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int index = gen_find_spec_index(decl, gen_mangled_type_symbol(q));

#line 1693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((index>(0-1)))
#line 1693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_complete_spec(index);
else
#line 1693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 1694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 1754 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_tagged_enum_decl(int id)
#line 1754 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int enum_name = node_value[id];

#line 1699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c_enum_name = sym_c_symbol(enum_name);

#line 1700 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tag_name = sym_c_symbol(sym_qualified(enum_name, sym_tag_id()));

#line 1701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 14);

#line 1702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 13);

#line 1703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, tag_name);

#line 1704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 13);

#line 1705 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[id];

#line 1713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((f!=0))
#line 1713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1707 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(sym_qualified(enum_name, node_a[f])));

#line 1708 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1709 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_INT, node_value[f]);

#line 1710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[f]!=0))
#line 1710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
else
#line 1710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];
}

#line 1714 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 14);

#line 1715 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, tag_name);

#line 1716 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1717 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1719 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 14);

#line 1720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 12);

#line 1721 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, c_enum_name);

#line 1722 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 13);

#line 1723 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, tag_name);

#line 1724 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 18);

#line 1725 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_tag_id());

#line 1726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 30);

#line 1728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 13);

#line 1729 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_a[id];

#line 1747 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((f!=0))
#line 1747 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1745 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_b[f]!=0))
#line 1745 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 12);

#line 1733 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 13);

#line 1734 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = node_b[f];

#line 1740 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((field!=0))
#line 1740 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_decl(node_b[field], node_a[field]);

#line 1737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1739 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
field = node_next[field];
}

#line 1741 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 14);

#line 1742 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_a[f]));

#line 1743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1744 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1745 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1746 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];
}

#line 1748 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 14);

#line 1749 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1750 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 14);

#line 1751 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, c_enum_name);

#line 1752 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1753 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}

#line 1780 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_enum_decl(int id)
#line 1780 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1757 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_pos = node_pos[id];

#line 1758 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_epoch = (gen_source_epoch+1);

#line 1759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tagged = 0;

#line 1760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int probe = node_a[id];

#line 1761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((probe!=0))
#line 1761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_b[probe]!=0))
#line 1761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tagged = 1;
else
#line 1761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
probe = node_next[probe];
}

#line 1762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tagged==1))
#line 1762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_tagged_enum_decl(id);

#line 1762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 1762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1763 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 14);

#line 1764 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 13);

#line 1765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_value[id]));

#line 1766 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 13);

#line 1767 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[id];

#line 1775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((f!=0))
#line 1775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(sym_qualified(node_value[id], node_a[f])));

#line 1770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 11);

#line 1771 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_INT, node_value[f]);

#line 1772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[f]!=0))
#line 1772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
else
#line 1772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1773 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1774 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];
}

#line 1776 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 14);

#line 1777 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_value[id]));

#line 1778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1779 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}

#line 1788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_extern_param(int ty, int name)
#line 1788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_STRING))
#line 1787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1784 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 16);

#line 1785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 3);

#line 1786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(name));
}
else
#line 1787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_decl(ty, name);
}

#line 1803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_function_signature(int id)
#line 1803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1792 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bi_has_flag(node_value[id], BI_FLAG_MAIN)==1))
#line 1791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 1);
else
#line 1792 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_aux[id], node_b[id], 0);

#line 1793 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_value[id]));

#line 1794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 1795 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int param = node_c[id];

#line 1801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((param!=0))
#line 1801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1798 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_EXTERN))
#line 1797 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_extern_param(node_b[param], node_a[param]);
else
#line 1798 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_decl(node_b[param], node_a[param]);

#line 1799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[param]!=0))
#line 1799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
else
#line 1799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1800 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
param = node_next[param];
}

#line 1802 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);
}

#line 1811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_prototype(int id)
#line 1811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_pos = node_pos[id];

#line 1807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_epoch = (gen_source_epoch+1);

#line 1808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_function_signature(id);

#line 1809 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}

#line 1833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_function(int id)
#line 1833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_pos = node_pos[id];

#line 1815 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_epoch = (gen_source_epoch+1);

#line 1816 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_function_signature(id);

#line 1832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((bi_has_flag(node_value[id], BI_FLAG_MAIN)==1)&&(node_kind[node_a[id]]==N_BLOCK)))
#line 1832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1818 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 13);

#line 1819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_emit_scope(emit_scope_depth);

#line 1820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_scope_start[emit_scope_depth] = emit_defer_count;

#line 1821 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_scope_depth = (emit_scope_depth+1);

#line 1822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[node_a[id]];

#line 1823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 1823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_stmt(item);

#line 1823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 1824 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_scope_depth = (emit_scope_depth-1);

#line 1825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_defer_from(emit_scope_start[emit_scope_depth]);

#line 1826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_defer_count = emit_scope_start[emit_scope_depth];

#line 1827 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 5);

#line 1828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_INT, 0);

#line 1829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1830 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1831 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 14);
}
else
#line 1832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_stmt(node_a[id]);
}

#line 1903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void gen_program(int id)
#line 1903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen_saved_node_count = node_count;

#line 1837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_pos = 0;

#line 1838 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_source_epoch = 0;

#line 1839 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_match_serial = 0;

#line 1840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_tuple_count = 0;

#line 1841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_reset();

#line 1841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_count = 0;

#line 1841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_name_override = 0;

#line 1841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_defer_count = 0;

#line 1841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_scope_depth = 0;

#line 1841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_loop_depth = 0;

#line 1842 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[id];

#line 1855 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 1855 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[item]==N_GLOBAL)||(node_kind[item]==N_CONST)))
#line 1844 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_stmt(item);
else
#line 1853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_STRUCT))
#line 1848 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1846 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sf = node_a[item];

#line 1847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((sf!=0))
#line 1847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(node_b[sf]);

#line 1847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sf = node_next[sf];
}
}
else
#line 1853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_FUNC))
#line 1853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1850 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(node_b[item]);

#line 1851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pp = node_c[item];

#line 1851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((pp!=0))
#line 1851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_type(node_b[pp]);

#line 1851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
pp = node_next[pp];
}

#line 1852 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_stmt(node_a[item]);
}
else
#line 1853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1854 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 1856 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tuple_i = 0;

#line 1860 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((tuple_i<gen_tuple_count))
#line 1860 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_tuple_decl(gen_tuple_type[tuple_i], gen_tuple_name[tuple_i]);

#line 1859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tuple_i = (tuple_i+1);
}

#line 1862 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_a[id];

#line 1868 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 1868 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1866 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_STRUCT))
#line 1866 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 14);

#line 1865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 12);

#line 1865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_value[item]));

#line 1865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 22);

#line 1865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, sym_c_symbol(node_value[item]));

#line 1865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1866 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1867 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 1869 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int si = 0;

#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((si<gen_spec_count))
#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_kind[si]==1))
#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 14);

#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_KW, 12);

#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_spec_name[si]);

#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 22);

#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_spec_name[si]);

#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);
}
else
#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
si = (si+1);
}

#line 1872 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int scan_si = 0;

#line 1884 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((scan_si<gen_spec_count))
#line 1884 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1882 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_kind[scan_si]==2))
#line 1882 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1875 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int scan_decl = gen_spec_decl[scan_si];

#line 1876 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int old_active_scan = gen_active_function;

#line 1877 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_decl(scan_decl, gen_spec_type[scan_si]);

#line 1878 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_active_function = scan_decl;

#line 1879 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_collect_stmt(node_a[scan_decl]);

#line 1880 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_active_function = old_active_scan;

#line 1881 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_clear();
}
else
#line 1882 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1883 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
scan_si = (scan_si+1);
}

#line 1886 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_struct_state((node_count+1));

#line 1887 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_spec_state(gen_spec_count);

#line 1888 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int reset_struct = 0;

#line 1889 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((reset_struct<gen_struct_state_cap))
#line 1889 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1889 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_struct_state[reset_struct] = 0;

#line 1889 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
reset_struct = (reset_struct+1);
}

#line 1890 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int reset_spec = 0;

#line 1891 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((reset_spec<gen_spec_count))
#line 1891 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1891 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_spec_state[reset_spec] = 0;

#line 1891 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
reset_spec = (reset_spec+1);
}

#line 1892 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_a[id];

#line 1893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 1893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_STRUCT))
#line 1893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_complete_struct(item);
else
#line 1893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_ENUM))
#line 1893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_enum_decl(item);
else
#line 1893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 1894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
si = 0;

#line 1894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((si<gen_spec_count))
#line 1894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_kind[si]==1))
#line 1894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_emit_complete_spec(si);
else
#line 1894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
si = (si+1);
}

#line 1896 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_a[id];

#line 1897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 1897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[item]==N_FUNC)||(node_kind[item]==N_EXTERN)))
#line 1897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_prototype(item);
else
#line 1897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
si = 0;

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((si<gen_spec_count))
#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_kind[si]==2))
#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_decl(gen_spec_decl[si], gen_spec_type[si]);

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int rr = gen_substitute_type(node_b[gen_spec_decl[si]]);

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_type(node_kind[rr], rr, node_value[rr]);

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_IDENT, gen_spec_name[si]);

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 6);

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pp = node_c[gen_spec_decl[si]];

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((pp!=0))
#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pt = gen_substitute_type(node_b[pp]);

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_decl(pt, node_a[pp]);

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_next[pp]!=0))
#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 7);
else
#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
pp = node_next[pp];
}

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 8);

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_PUNCT, 12);

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_emit(C_NEWLINE, 0);

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_clear();
}
else
#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
si = (si+1);
}

#line 1899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_a[id];

#line 1899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 1899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[item]==N_GLOBAL)||(node_kind[item]==N_CONST)))
#line 1899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_stmt(item);
else
#line 1899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 1900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
si = 0;

#line 1900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((si<gen_spec_count))
#line 1900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((gen_spec_kind[si]==2))
#line 1900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_function_specialized(gen_spec_decl[si], gen_spec_type[si], gen_spec_name[si]);
else
#line 1900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
si = (si+1);
}

#line 1901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_a[id];

#line 1901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 1901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_FUNC))
#line 1901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_function(item);
else
#line 1901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 1902 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_count = gen_saved_node_count;
}

#line 1920 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int build_regression_ast()
#line 1920 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1908 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_count = 1;

#line 1909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_count = 1;

#line 1910 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int two = ast_node(N_INT, 0, 0, 0, 2, 0);

#line 1911 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int three = ast_node(N_INT, 0, 0, 0, 3, 0);

#line 1912 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sum = ast_node(N_BINOP, two, three, 0, OP_ADD, 0);

#line 1913 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int print_stmt = ast_node(N_PRINT, sum, 0, 0, 0, 0);

#line 1914 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int zero = ast_node(N_INT, 0, 0, 0, 0, 0);

#line 1915 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ret = ast_node(N_RETURN, zero, 0, 0, 0, 0);

#line 1916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int body_items = ast_link(print_stmt, ret);

#line 1917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int body = ast_node(N_BLOCK, body_items, 0, 0, 0, 0);

#line 1918 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int main_fn = ast_node(N_FUNC, body, 0, 0, 1, TY_INT);

#line 1919 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_PROGRAM, main_fn, 0, 0, 0, 0);
}

#line 1943 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void generator_regression_main()
#line 1943 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int program = build_regression_ast();

#line 1924 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_program(program);

#line 1925 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int first_count = code_count;

#line 1926 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_snapshot(first_count);

#line 1927 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 1932 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<first_count))
#line 1932 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1929 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
snapshot_kind[i] = code_kind[i];

#line 1930 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
snapshot_value[i] = code_value[i];

#line 1931 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 1933 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_program(program);

#line 1934 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int same = 1;

#line 1935 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = 0;

#line 1936 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((code_count!=first_count))
#line 1936 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
same = 0;
else
#line 1936 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1941 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<first_count))
#line 1941 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 1938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((code_kind[i]!=snapshot_kind[i]))
#line 1938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
same = 0;
else
#line 1938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((code_value[i]!=snapshot_value[i]))
#line 1939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
same = 0;
else
#line 1939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 1940 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 1942 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((same==1))
#line 1942 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "generator_regression: PASS");
else
#line 1942 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "generator_regression: FAIL");
}

#line 2046 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void input_reset()
#line 2046 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2044 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_count = 0;

#line 2045 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = 0;
}

#line 2055 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void input_put(int kind, int value, int text, int pos)
#line 2055 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2049 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_input(input_count);

#line 2050 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_kind[input_count] = kind;

#line 2051 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_value[input_count] = value;

#line 2052 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_text[input_count] = text;

#line 2053 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_source_pos[input_count] = pos;

#line 2054 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_count = (input_count+1);
}

#line 2060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int input_peek()
#line 2060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2059 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_pos<input_count))
#line 2058 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return input_kind[input_pos];
else
#line 2059 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_EOF;
}

#line 2065 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int input_payload()
#line 2065 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2064 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_pos<input_count))
#line 2063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return input_value[input_pos];
else
#line 2064 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 2070 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int input_text_payload()
#line 2070 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2069 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_pos<input_count))
#line 2068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return input_text[input_pos];
else
#line 2069 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 2077 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int input_take(int kind)
#line 2077 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2076 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==kind))
#line 2076 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2074 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2075 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 2076 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 2088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_generic_param(int name)
#line 2088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2082 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = ast_generic_scope;

#line 2086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 2086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[p]==name))
#line 2084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 2084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2085 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 2087 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 2103 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_generic_params()
#line 2103 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2091 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int params = 0;

#line 2092 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LT)==0))
#line 2092 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2092 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2100 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((1==1))
#line 2100 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2095 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = input_payload();

#line 2095 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = ast_node(N_PARAM, name, 0, 0, 0, 0);

#line 2097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((params==0))
#line 2097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
params = p;
else
#line 2097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
params = ast_link(params, p);

#line 2099 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COMMA)==1))
#line 2098 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 2099 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
break;
}

#line 2101 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_GT)==0))
#line 2101 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2101 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2102 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return params;
}

#line 2233 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_type()
#line 2233 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int base = 0;

#line 2107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int named = 0;

#line 2108 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ty = 0;

#line 2135 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==1))
#line 2135 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int first = ast_type();

#line 2111 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((first==0))
#line 2111 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2111 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2125 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COMMA)==1))
#line 2122 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2113 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int items = first;

#line 2119 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((1==1))
#line 2119 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2115 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = ast_type();

#line 2116 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((item==0))
#line 2116 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2116 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = ast_link(items, item);

#line 2118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COMMA)==0))
#line 2118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
break;
}
else
#line 2118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 2120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2121 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = ast_node(TY_TUPLE, items, 0, 0, 0, 0);
}
else
#line 2125 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2123 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2123 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2123 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2124 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = first;
}

#line 2126 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_take(T_STAR)==1))
#line 2126 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2126 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = ast_node(TY_PTR, ty, 0, 0, 0, 0);
}

#line 2133 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_take(T_LBRACK)==1))
#line 2133 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2128 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_INT))
#line 2128 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2128 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2129 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int size = input_payload();

#line 2129 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2130 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((size<0))
#line 2130 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2130 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2131 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RBRACK)==0))
#line 2131 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2131 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2132 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = ast_node(TY_ARRAY, ty, 0, 0, size, 0);
}

#line 2134 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ty;
}
else
#line 2135 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2154 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_FN)==1))
#line 2154 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2137 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==0))
#line 2137 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2137 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2138 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int args = 0;

#line 2148 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_RPAREN))
#line 2148 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2140 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int at = ast_type();

#line 2141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((at==0))
#line 2141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2142 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = at;

#line 2147 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_take(T_COMMA)==1))
#line 2147 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
at = ast_type();

#line 2145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((at==0))
#line 2145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = ast_link(args, at);
}
}
else
#line 2148 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2149 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2149 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2149 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COLON)==0))
#line 2150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2151 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ret = ast_type();

#line 2152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ret==0))
#line 2152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2153 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_FUN, args, ret, 0, 0, 0);
}
else
#line 2154 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2221 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((input_peek()==T_ARRAY)&&((input_pos+1)<input_count))&&(input_kind[(input_pos+1)]==T_SCOPE)))
#line 2175 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ns = input_payload();

#line 2156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+2);

#line 2157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((input_peek()!=T_ID)&&(input_peek()!=T_ARRAY)))
#line 2157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int rhs = input_payload();

#line 2158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2159 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int named_type = sym_qualified(ns, rhs);

#line 2174 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LT)==1))
#line 2174 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2161 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int args = 0;

#line 2171 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_GT))
#line 2171 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2163 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int at = ast_type();

#line 2164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((at==0))
#line 2164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2165 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = at;

#line 2170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_take(T_COMMA)==1))
#line 2170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
at = ast_type();

#line 2168 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((at==0))
#line 2168 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2168 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = ast_link(args, at);
}
}
else
#line 2171 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2172 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_GT)==0))
#line 2172 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2172 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2173 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = ast_node(TY_GENERIC, args, 0, 0, named_type, 0);
}
else
#line 2174 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = ast_node(TY_NAMED, 0, 0, 0, named_type, 0);
}
else
#line 2221 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TINT)==1))
#line 2176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_INT;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TBOOL)==1))
#line 2177 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_BOOL;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TSTRING)==1))
#line 2178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_STRING;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TCHAR)==1))
#line 2179 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_CHAR;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_FLOAT)==1))
#line 2180 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_FLOAT;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TDOUBLE)==1))
#line 2181 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_DOUBLE;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TLONG)==1))
#line 2182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TLONG)==1))
#line 2182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_LLONG;
else
#line 2182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_LONG;
}
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TU8)==1))
#line 2183 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_U8;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TU16)==1))
#line 2184 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_U16;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TU32)==1))
#line 2185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_U32;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TU64)==1))
#line 2186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_U64;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TI8)==1))
#line 2187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_I8;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TI16)==1))
#line 2188 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_I16;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TI32)==1))
#line 2189 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_I32;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TI64)==1))
#line 2190 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_I64;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TUSIZE)==1))
#line 2191 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_USIZE;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TVOID)==1))
#line 2192 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base = TY_VOID;
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_ID))
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
named = input_payload();

#line 2195 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2200 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SCOPE)==1))
#line 2200 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2197 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2197 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2197 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2198 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int rhs = input_payload();

#line 2198 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
named = sym_qualified(named, rhs);
}
else
#line 2200 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
named = ast_type_name(named);

#line 2218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_LT))
#line 2217 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int args = 0;

#line 2204 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_GT))
#line 2214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int at = ast_type();

#line 2207 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((at==0))
#line 2207 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2207 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2208 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = at;

#line 2213 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_take(T_COMMA)==1))
#line 2213 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2210 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
at = ast_type();

#line 2211 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((at==0))
#line 2211 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2211 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = ast_link(args, at);
}
}
else
#line 2214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_GT)==0))
#line 2215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2216 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = ast_node(TY_GENERIC, args, 0, 0, named, 0);
}
else
#line 2218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ast_generic_param(named)==1))
#line 2217 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = ast_node(TY_PARAM, 0, 0, 0, named, 0);
else
#line 2218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = ast_node(TY_NAMED, 0, 0, 0, named, 0);
}
else
#line 2219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;

#line 2220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 2220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = ast_node(base, 0, 0, 0, named, 0);
else
#line 2220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 2224 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_take(T_STAR)==1))
#line 2224 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = ast_node(TY_PTR, ty, 0, 0, 0, 0);
}

#line 2231 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_take(T_LBRACK)==1))
#line 2231 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_INT))
#line 2226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2227 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int size = input_payload();

#line 2228 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2229 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RBRACK)==0))
#line 2229 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2229 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ty = ast_node(TY_ARRAY, ty, 0, 0, size, 0);
}

#line 2232 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ty;
}

#line 2249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_call_args()
#line 2249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2236 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int args = 0;

#line 2246 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_RPAREN))
#line 2246 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2238 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arg = ast_expr();

#line 2239 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((arg<0))
#line 2239 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2239 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = arg;

#line 2245 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_take(T_COMMA)==1))
#line 2245 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arg = ast_expr();

#line 2243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((arg<0))
#line 2243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2244 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = ast_link(args, arg);
}
}
else
#line 2246 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2248 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return args;
}

#line 2322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_primary()
#line 2322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2255 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_CHAR))
#line 2255 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int value = input_payload();

#line 2253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2254 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_CHAR, 0, 0, 0, value, 0);
}
else
#line 2255 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_NULL)==1))
#line 2256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_NULL, 0, 0, 0, 0, 0);
else
#line 2256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2257 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_FLOAT))
#line 2257 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2257 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int value = input_payload();

#line 2257 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2257 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_FLOAT, 0, 0, 0, value, 0);
}
else
#line 2257 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_INT))
#line 2263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2259 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int value = input_payload();

#line 2260 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int raw = input_text_payload();

#line 2261 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2262 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_INT, 0, 0, 0, value, raw);
}
else
#line 2263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2268 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_STRING))
#line 2268 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2265 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int value = input_payload();

#line 2266 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2267 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_STRING, 0, 0, 0, value, 0);
}
else
#line 2268 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2269 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_TRUE)==1))
#line 2269 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_BOOL, 0, 0, 0, 1, 0);
else
#line 2269 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2270 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_FALSE)==1))
#line 2270 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_BOOL, 0, 0, 0, 0, 0);
else
#line 2270 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2298 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((input_peek()==T_ID)||(input_peek()==T_ARRAY)))
#line 2298 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2272 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = input_payload();

#line 2273 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2278 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_take(T_SCOPE)==1))
#line 2278 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2275 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((input_peek()!=T_ID)&&(input_peek()!=T_ARRAY)))
#line 2275 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2275 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int rhs = input_payload();

#line 2276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2277 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
name = sym_qualified(name, rhs);
}

#line 2283 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==1))
#line 2283 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2280 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int args = ast_call_args();

#line 2281 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((args<0))
#line 2281 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2281 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2282 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_CALL, args, 0, 0, name, 0);
}
else
#line 2283 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2284 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int base = ast_node(N_VAR, 0, 0, 0, name, 0);

#line 2296 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_DOT)==1))
#line 2296 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2286 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2286 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2286 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2287 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = input_payload();

#line 2288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2289 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field_expr = ast_node(N_FIELD_ACCESS, base, 0, 0, field, 0);

#line 2294 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==1))
#line 2294 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2291 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int args = ast_call_args();

#line 2292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((args<0))
#line 2292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2293 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_INDIRECT_CALL, field_expr, args, 0, 0, 0);
}
else
#line 2294 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2295 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return field_expr;
}
else
#line 2296 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2297 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return base;
}
else
#line 2298 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2320 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==1))
#line 2320 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2300 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int e = ast_expr();

#line 2301 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e<0))
#line 2301 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2301 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COMMA)==1))
#line 2312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int items = e;

#line 2309 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((1==1))
#line 2309 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = ast_expr();

#line 2306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((item<0))
#line 2306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2307 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = ast_link(items, item);

#line 2308 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COMMA)==0))
#line 2308 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2308 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
break;
}
else
#line 2308 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 2310 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2310 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2310 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2311 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_TUPLE, items, 0, 0, 0, 0);
}
else
#line 2312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2313 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2313 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2313 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2318 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==1))
#line 2318 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2315 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int args = ast_call_args();

#line 2316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((args<0))
#line 2316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2317 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_INDIRECT_CALL, e, args, 0, 0, 0);
}
else
#line 2318 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return e;
}
else
#line 2320 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2321 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
}

#line 2343 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_unary()
#line 2343 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2330 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_MINUS)==1))
#line 2330 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2326 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int e = ast_unary();

#line 2327 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e<0))
#line 2327 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2327 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2328 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int zero = ast_node(N_INT, 0, 0, 0, 0, 0);

#line 2329 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_BINOP, zero, e, 0, OP_SUB, 0);
}
else
#line 2330 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_BITNOT)==1))
#line 2331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int e = ast_unary();

#line 2331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e<0))
#line 2331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int allbits = ast_node(N_INT, 0, 0, 0, (0-1), 0);

#line 2331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_BINOP, e, allbits, 0, OP_BITXOR, 0);
}
else
#line 2331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2336 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_STAR)==1))
#line 2336 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int e = ast_unary();

#line 2334 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e<0))
#line 2334 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2334 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2335 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_DEREF, e, 0, 0, 0, 0);
}
else
#line 2336 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2341 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_AMP)==1))
#line 2341 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2338 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int e = ast_unary();

#line 2339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e<0))
#line 2339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2340 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_ADDRESS, e, 0, 0, 0, 0);
}
else
#line 2341 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_primary();
}

#line 2364 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_precedence(int kind)
#line 2364 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2346 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_OR_OR))
#line 2346 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 2346 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2347 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_AND_AND))
#line 2347 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 2;
else
#line 2347 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2348 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_EQEQ))
#line 2348 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 3;
else
#line 2348 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2349 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_NEQ))
#line 2349 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 3;
else
#line 2349 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2350 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_LT))
#line 2350 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 3;
else
#line 2350 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2351 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_GT))
#line 2351 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 3;
else
#line 2351 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_CONCAT))
#line 2352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 4;
else
#line 2352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2353 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_BITOR))
#line 2353 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 5;
else
#line 2353 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_BITXOR))
#line 2354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 6;
else
#line 2354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2355 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_AMP))
#line 2355 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 7;
else
#line 2355 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2356 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_SHL))
#line 2356 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 8;
else
#line 2356 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_SHR))
#line 2357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 8;
else
#line 2357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2358 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_PLUS))
#line 2358 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 9;
else
#line 2358 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_MINUS))
#line 2359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 9;
else
#line 2359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_STAR))
#line 2360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 10;
else
#line 2360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_DIVIDE))
#line 2361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 10;
else
#line 2361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2362 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_MOD))
#line 2362 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 10;
else
#line 2362 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2363 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 2385 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_operator(int kind)
#line 2385 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_PLUS))
#line 2367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_ADD;
else
#line 2367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_MINUS))
#line 2368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_SUB;
else
#line 2368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_STAR))
#line 2369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_MUL;
else
#line 2369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_DIVIDE))
#line 2370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_DIV;
else
#line 2370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2371 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_MOD))
#line 2371 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_MOD;
else
#line 2371 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2372 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_EQEQ))
#line 2372 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_EQ;
else
#line 2372 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2373 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_NEQ))
#line 2373 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_NEQ;
else
#line 2373 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2374 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_LT))
#line 2374 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_LT;
else
#line 2374 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_GT))
#line 2375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_GT;
else
#line 2375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2376 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_AND_AND))
#line 2376 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_AND;
else
#line 2376 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2377 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_OR_OR))
#line 2377 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_OR;
else
#line 2377 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2378 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_CONCAT))
#line 2378 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_CONCAT;
else
#line 2378 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2379 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_AMP))
#line 2379 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_BITAND;
else
#line 2379 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2380 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_BITOR))
#line 2380 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_BITOR;
else
#line 2380 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_BITXOR))
#line 2381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_BITXOR;
else
#line 2381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2382 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_SHL))
#line 2382 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_SHL;
else
#line 2382 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2383 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_SHR))
#line 2383 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_SHR;
else
#line 2383 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2384 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_GT;
}

#line 2399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_compound_operator(int kind)
#line 2399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2388 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_PLUS_EQ))
#line 2388 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_ADD;
else
#line 2388 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2389 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_MINUS_EQ))
#line 2389 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_SUB;
else
#line 2389 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2390 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_STAR_EQ))
#line 2390 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_MUL;
else
#line 2390 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2391 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_DIV_EQ))
#line 2391 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_DIV;
else
#line 2391 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_MOD_EQ))
#line 2392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_MOD;
else
#line 2392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2393 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_AMP_EQ))
#line 2393 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_BITAND;
else
#line 2393 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2394 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_BITOR_EQ))
#line 2394 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_BITOR;
else
#line 2394 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_BITXOR_EQ))
#line 2395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_BITXOR;
else
#line 2395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2396 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_SHL_EQ))
#line 2396 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_SHL;
else
#line 2396 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==T_SHR_EQ))
#line 2397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return OP_SHR;
else
#line 2397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 2406 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_take_compound_operator()
#line 2406 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int kind = input_peek();

#line 2403 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int op = ast_compound_operator(kind);

#line 2404 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((op!=0))
#line 2404 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);
else
#line 2404 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2405 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return op;
}

#line 2410 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_compound_assign(int left, int op, int right)
#line 2410 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2409 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_COMPOUND_ASSIGN, left, right, 0, op, 0);
}

#line 2437 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_expr_prec(int min_prec)
#line 2437 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2413 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int left = ast_unary();

#line 2414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((left<0))
#line 2414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2435 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((1==1))
#line 2435 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2434 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LBRACK)==1))
#line 2421 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int index = ast_expr();

#line 2418 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((index<0))
#line 2418 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2418 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2419 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RBRACK)==0))
#line 2419 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2419 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2420 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
left = ast_node(N_INDEX, left, index, 0, 0, 0);
}
else
#line 2434 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_DOT)==1))
#line 2426 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2423 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = input_payload();

#line 2424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2425 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
left = ast_node(N_FIELD_ACCESS, left, 0, 0, field, 0);
}
else
#line 2434 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2427 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = ast_precedence(input_peek());

#line 2428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((p<min_prec))
#line 2428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return left;
else
#line 2428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int op_token = input_peek();

#line 2430 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2431 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int right = ast_expr_prec((p+1));

#line 2432 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((right<0))
#line 2432 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2432 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2433 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
left = ast_node(N_BINOP, left, right, 0, ast_operator(op_token), 0);
}
}

#line 2436 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return left;
}

#line 2441 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_expr()
#line 2441 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2440 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_expr_prec(1);
}

#line 2445 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int clone_for_step(int step)
#line 2445 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2444 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(node_kind[step], node_a[step], node_b[step], node_c[step], node_value[step], node_aux[step]);
}

#line 2469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int lower_for_stmt(int id, int step)
#line 2469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_CONTINUE))
#line 2452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2449 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int s = clone_for_step(step);

#line 2450 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c = ast_node(N_CONTINUE, 0, 0, 0, 0, 0);

#line 2451 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_BLOCK, ast_link(s, c), 0, 0, 0, 0);
}
else
#line 2452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2457 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_IF))
#line 2457 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2454 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int yes = lower_for_stmt(node_b[id], step);

#line 2455 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int no = lower_for_stmt(node_c[id], step);

#line 2456 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_IF, node_a[id], yes, no, node_value[id], node_aux[id]);
}
else
#line 2457 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_BLOCK))
#line 2467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2459 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[id];

#line 2460 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int out = 0;

#line 2465 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 2465 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2462 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int x = lower_for_stmt(item, step);

#line 2463 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((out==0))
#line 2463 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
out = x;
else
#line 2463 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
out = ast_link(out, x);

#line 2464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 2466 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_BLOCK, out, 0, 0, 0, 0);
}
else
#line 2467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2468 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
}

#line 2486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_alignment()
#line 2486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_ALIGNAS)==0))
#line 2472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==0))
#line 2473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2474 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_INT))
#line 2474 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2474 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2475 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int alignment = input_payload();

#line 2476 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2477 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((alignment<1))
#line 2477 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2477 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2478 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int power = 1;

#line 2482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((power<alignment))
#line 2482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((power>1073741824))
#line 2480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
power = (power*2);
}

#line 2483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((power!=alignment))
#line 2483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return alignment;
}

#line 2706 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_stmt()
#line 2706 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_DEFER)==1))
#line 2494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2490 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int cleanup = ast_expr();

#line 2491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((cleanup<0))
#line 2491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2492 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2492 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2492 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2493 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_DEFER, cleanup, 0, 0, 0, 0);
}
else
#line 2494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2524 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_MATCH)==1))
#line 2524 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int subject = ast_expr();

#line 2497 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((subject<0))
#line 2497 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2497 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LBRACE)==0))
#line 2498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2499 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arms = 0;

#line 2521 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_peek()!=T_RBRACE))
#line 2521 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((input_peek()==T_EOF)||(input_peek()!=T_ID)))
#line 2501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2502 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int variant = input_payload();

#line 2502 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2503 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bindings = 0;

#line 2515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==1))
#line 2515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int binding_name = input_payload();

#line 2506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bindings = ast_node(N_VAR, 0, 0, 0, binding_name, 0);

#line 2513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COMMA)==1))
#line 2513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2509 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2509 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2509 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2510 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int second_binding_name = input_payload();

#line 2510 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int second_binding = ast_node(N_VAR, 0, 0, 0, second_binding_name, 0);

#line 2512 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bindings = ast_link(bindings, second_binding);
}
else
#line 2513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2514 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2514 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2514 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 2515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2516 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_FATARROW)==0))
#line 2516 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2516 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2517 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int body = ast_stmt();

#line 2518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((body<0))
#line 2518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2519 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arm = ast_node(N_MATCH_ARM, bindings, body, 0, variant, 0);

#line 2520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((arms==0))
#line 2520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arms = arm;
else
#line 2520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arms = ast_link(arms, arm);
}

#line 2522 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RBRACE)==0))
#line 2522 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2522 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2523 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_MATCH, subject, arms, 0, 0, 0);
}
else
#line 2524 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2528 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_BREAK)==1))
#line 2528 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2527 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_BREAK, 0, 0, 0, 0, 0);
}
else
#line 2528 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2537 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_CONTINUE)==1))
#line 2537 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2535 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((for_step_context!=0))
#line 2535 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2532 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int s = clone_for_step(for_step_context);

#line 2533 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c = ast_node(N_CONTINUE, 0, 0, 0, 0, 0);

#line 2534 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_BLOCK, ast_link(s, c), 0, 0, 0, 0);
}
else
#line 2535 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2536 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_CONTINUE, 0, 0, 0, 0, 0);
}
else
#line 2537 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2570 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LET)==1))
#line 2570 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==1))
#line 2556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2541 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int first_name = input_payload();

#line 2541 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2542 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COMMA)==0))
#line 2542 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2542 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2543 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2543 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2543 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int second_name = input_payload();

#line 2544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2545 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2545 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2545 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2546 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COLON)==0))
#line 2546 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2546 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2547 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tuple_ty = ast_type();

#line 2548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tuple_ty==0))
#line 2548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_EQUAL)==0))
#line 2549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tuple_value = ast_expr();

#line 2551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tuple_value<0))
#line 2551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2553 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int first_binding = ast_node(N_VAR, 0, 0, 0, first_name, 0);

#line 2554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int second_binding = ast_node(N_VAR, 0, 0, 0, second_name, 0);

#line 2555 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_TUPLE_BIND, ast_link(first_binding, second_binding), tuple_ty, tuple_value, 0, 0);
}
else
#line 2556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2558 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = input_payload();

#line 2559 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COLON)==0))
#line 2560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2561 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ty = ast_type();

#line 2562 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 2562 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2562 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2563 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int alignment = ast_alignment();

#line 2564 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((alignment<0))
#line 2564 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2564 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2565 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_EQUAL)==0))
#line 2565 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2565 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2566 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int value = ast_expr();

#line 2567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value<0))
#line 2567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2568 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2568 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2568 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2569 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_LET, name, ty, value, 0, alignment);
}
else
#line 2570 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2576 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_PRINT)==1))
#line 2576 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2572 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int value = ast_expr();

#line 2573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value<0))
#line 2573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2574 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2574 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2574 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2575 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_PRINT, value, 0, 0, 0, 0);
}
else
#line 2576 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2591 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_IF)==1))
#line 2591 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2578 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int cond = ast_expr();

#line 2579 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((cond<0))
#line 2579 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2579 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2580 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_THEN)==0))
#line 2580 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2580 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int yes = ast_stmt();

#line 2582 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((yes<0))
#line 2582 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2582 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2583 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int no = 0;

#line 2589 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_ELSE)==1))
#line 2587 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2585 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
no = ast_stmt();

#line 2586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((no<0))
#line 2586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 2589 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2588 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
no = ast_node(N_BLOCK, 0, 0, 0, 0, 0);
}

#line 2590 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_IF, cond, yes, no, 0, 0);
}
else
#line 2591 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_FOR)==1))
#line 2653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2593 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==0))
#line 2593 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2593 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2594 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int init = 0;

#line 2620 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_SEMI))
#line 2620 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LET)==1))
#line 2607 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = input_payload();

#line 2598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2599 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COLON)==0))
#line 2599 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2599 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2600 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int t = ast_type();

#line 2601 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((t==0))
#line 2601 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2601 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int alignment = ast_alignment();

#line 2603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((alignment<0))
#line 2603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2604 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_EQUAL)==0))
#line 2604 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2604 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2605 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int v = ast_expr();

#line 2606 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
init = ast_node(N_LET, n, t, v, 0, alignment);
}
else
#line 2619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int l = ast_expr();

#line 2609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((l<0))
#line 2609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2618 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_EQUAL)==1))
#line 2613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2611 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int r = ast_expr();

#line 2612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
init = ast_node(N_ASSIGN, l, r, 0, 0, 0);
}
else
#line 2618 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2614 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int cop = ast_take_compound_operator();

#line 2615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((cop==0))
#line 2615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int r = ast_expr();

#line 2617 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
init = ast_compound_assign(l, cop, r);
}
}
}
else
#line 2620 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2622 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int cond = ast_expr();

#line 2623 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2623 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2623 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int step = 0;

#line 2638 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_RPAREN))
#line 2638 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2626 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int l = ast_expr();

#line 2627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((l<0))
#line 2627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2637 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_EQUAL)==1))
#line 2631 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2629 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int r = ast_expr();

#line 2630 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
step = ast_node(N_ASSIGN, l, r, 0, 0, 0);
}
else
#line 2637 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int cop = ast_take_compound_operator();

#line 2636 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((cop!=0))
#line 2636 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2634 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int r = ast_expr();

#line 2635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
step = ast_compound_assign(l, cop, r);
}
else
#line 2636 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
step = ast_node(N_EXPR, l, 0, 0, 0, 0);
}
}
else
#line 2638 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
step = ast_node(N_BLOCK, 0, 0, 0, 0, 0);

#line 2639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2640 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LBRACE)==0))
#line 2640 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2640 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2641 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
for_step_context = step;

#line 2642 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int items = 0;

#line 2648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_peek()!=T_RBRACE))
#line 2648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2644 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_EOF))
#line 2644 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2644 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2645 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int x = ast_stmt();

#line 2646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((x<0))
#line 2646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((items==0))
#line 2647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = x;
else
#line 2647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = ast_link(items, x);
}

#line 2649 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RBRACE)==0))
#line 2649 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2649 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
for_step_context = 0;

#line 2651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int body = ast_node(N_BLOCK, items, 0, 0, 0, 0);

#line 2652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_FOR, init, cond, body, step, 0);
}
else
#line 2653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2668 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_WHILE)==1))
#line 2668 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int cond = ast_expr();

#line 2656 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((cond<0))
#line 2656 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2656 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2657 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LBRACE)==0))
#line 2657 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2657 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2658 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int items = 0;

#line 2664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_peek()!=T_RBRACE))
#line 2664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_EOF))
#line 2660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2661 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = ast_stmt();

#line 2662 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((item<0))
#line 2662 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2662 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((items==0))
#line 2663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = item;
else
#line 2663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = ast_link(items, item);
}

#line 2665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RBRACE)==0))
#line 2665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int body = ast_node(N_BLOCK, items, 0, 0, 0, 0);

#line 2667 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_WHILE, cond, body, 0, 0, 0);
}
else
#line 2668 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LBRACE)==1))
#line 2679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2670 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int items = 0;

#line 2676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_peek()!=T_RBRACE))
#line 2676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2672 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_EOF))
#line 2672 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2672 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2673 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = ast_stmt();

#line 2674 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((item<0))
#line 2674 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2674 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2675 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((items==0))
#line 2675 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = item;
else
#line 2675 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = ast_link(items, item);
}

#line 2677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RBRACE)==0))
#line 2677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_BLOCK, items, 0, 0, 0, 0);
}
else
#line 2679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RETURN)==1))
#line 2688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2681 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int value = 0;

#line 2685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_SEMI))
#line 2685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2683 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
value = ast_expr();

#line 2684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value<0))
#line 2684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 2685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_RETURN, value, 0, 0, 0, 0);
}
else
#line 2688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2689 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int left = ast_expr();

#line 2690 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((left<0))
#line 2690 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2690 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_EQUAL)==1))
#line 2696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2692 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int right = ast_expr();

#line 2693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((right<0))
#line 2693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_ASSIGN, left, right, 0, 0, 0);
}
else
#line 2696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2697 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int compound_op = ast_take_compound_operator();

#line 2703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((compound_op!=0))
#line 2703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int right_compound = ast_expr();

#line 2700 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((right_compound<0))
#line 2700 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2700 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_compound_assign(left, compound_op, right_compound);
}
else
#line 2703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2705 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_EXPR, left, 0, 0, 0, 0);
}

#line 2722 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_params()
#line 2722 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2709 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int params = 0;

#line 2710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_RPAREN))
#line 2710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 2710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2721 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((1==1))
#line 2721 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = input_payload();

#line 2714 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2715 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COLON)==0))
#line 2715 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2715 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2716 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ty = ast_type();

#line 2717 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 2717 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2717 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2718 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int param = ast_node(N_PARAM, name, ty, 0, 0, node_kind[ty]);

#line 2719 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((params==0))
#line 2719 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
params = param;
else
#line 2719 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
params = ast_link(params, param);

#line 2720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COMMA)==0))
#line 2720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return params;
else
#line 2720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
}

#line 2753 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_struct_decl()
#line 2753 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2725 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2725 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2725 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = input_payload();

#line 2726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
name = ast_decl_name(name);

#line 2728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int params = 0;

#line 2729 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int old_scope = ast_generic_scope;

#line 2734 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_LT))
#line 2734 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2731 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
params = ast_generic_params();

#line 2732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((params<0))
#line 2732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2733 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_generic_scope = params;
}
else
#line 2734 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2735 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LBRACE)==0))
#line 2735 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2735 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int fields = 0;

#line 2747 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_peek()!=T_RBRACE))
#line 2747 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_EOF))
#line 2738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2739 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2739 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2739 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2740 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field_name = input_payload();

#line 2740 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2741 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COLON)==0))
#line 2741 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2741 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2742 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field_type = ast_type();

#line 2743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((field_type==0))
#line 2743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2744 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2744 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2744 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2745 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = ast_node(N_FIELD, field_name, field_type, 0, 0, 0);

#line 2746 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((fields==0))
#line 2746 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
fields = field;
else
#line 2746 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
fields = ast_link(fields, field);
}

#line 2748 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RBRACE)==0))
#line 2748 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2748 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2749 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==1))
#line 2749 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 2749 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2750 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_generic_scope = old_scope;

#line 2751 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((params==0))
#line 2751 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_STRUCT, fields, 0, 0, name, 0);
else
#line 2751 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2752 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_GENERIC_STRUCT, fields, 0, params, name, 0);
}

#line 2787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_enum_decl()
#line 2787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2756 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2756 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2756 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2757 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = input_payload();

#line 2757 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2758 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
name = ast_decl_name(name);

#line 2759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LBRACE)==0))
#line 2759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int values = 0;

#line 2761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ordinal = 0;

#line 2783 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_peek()!=T_RBRACE))
#line 2783 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2763 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_EOF))
#line 2763 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2763 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2764 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2764 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2764 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int member = input_payload();

#line 2765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2766 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int payload = 0;

#line 2778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LBRACE)==1))
#line 2778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2776 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_peek()!=T_RBRACE))
#line 2776 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((input_peek()==T_EOF)||(input_peek()!=T_ID)))
#line 2769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field_name = input_payload();

#line 2770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2771 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COLON)==0))
#line 2771 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2771 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field_type = ast_type();

#line 2773 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((field_type==0)||(input_take(T_SEMI)==0)))
#line 2773 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2773 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2774 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = ast_node(N_FIELD, field_name, field_type, 0, 0, 0);

#line 2775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((payload==0))
#line 2775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload = field;
else
#line 2775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload = ast_link(payload, field);
}

#line 2777 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RBRACE)==0))
#line 2777 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2777 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 2778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2779 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = ast_node(N_FIELD, member, payload, 0, ordinal, 0);

#line 2780 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((values==0))
#line 2780 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
values = item;
else
#line 2780 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
values = ast_link(values, item);

#line 2781 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ordinal = (ordinal+1);

#line 2782 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COMMA)==0))
#line 2782 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 2782 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 2784 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RBRACE)==0))
#line 2784 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2784 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==1))
#line 2785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 2785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_ENUM, values, 0, 0, name, 0);
}

#line 2808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_namespace_decl()
#line 2808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2790 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((input_peek()!=T_ID)&&(input_peek()!=T_ARRAY)))
#line 2790 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2790 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int raw = input_payload();

#line 2791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2792 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ns = raw;

#line 2793 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ast_namespace_scope!=0))
#line 2793 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ns = sym_qualified(ast_namespace_scope, raw);
else
#line 2793 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LBRACE)==0))
#line 2794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2795 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int old_ns = ast_namespace_scope;

#line 2796 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_namespace_scope = ns;

#line 2797 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int items = 0;

#line 2803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_peek()!=T_RBRACE))
#line 2803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_EOF))
#line 2799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_namespace_scope = old_ns;

#line 2799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
}
else
#line 2799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2800 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = ast_decl();

#line 2801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((item<0))
#line 2801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_namespace_scope = old_ns;

#line 2801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
}
else
#line 2801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2802 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((items==0))
#line 2802 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = item;
else
#line 2802 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = ast_link(items, item);
}

#line 2804 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RBRACE)==0))
#line 2804 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2804 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_namespace_scope = old_ns;

#line 2804 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
}
else
#line 2804 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2805 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==1))
#line 2805 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 2805 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_namespace_scope = old_ns;

#line 2807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_LIST, items, 0, 0, 0, 0);
}

#line 2884 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_decl()
#line 2884 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_NAMESPACE)==1))
#line 2811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_namespace_decl();
else
#line 2811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_EXTERN)==1))
#line 2826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_FUNC)==0))
#line 2813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2815 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = input_payload();

#line 2815 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2816 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
name = ast_decl_name(name);

#line 2817 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==0))
#line 2817 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2817 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2818 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int params = ast_params();

#line 2819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((params<0))
#line 2819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2821 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COLON)==0))
#line 2821 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2821 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ret_ty = ast_type();

#line 2823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ret_ty==0))
#line 2823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2824 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2824 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2824 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_EXTERN, 0, ret_ty, params, name, node_kind[ret_ty]);
}
else
#line 2826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2839 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_CONST)==1))
#line 2839 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = input_payload();

#line 2829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2830 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
name = ast_decl_name(name);

#line 2831 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COLON)==0))
#line 2831 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2831 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ty = ast_type();

#line 2832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 2832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int alignment = ast_alignment();

#line 2834 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((alignment<0))
#line 2834 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2834 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_EQUAL)==0))
#line 2835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int value = ast_expr();

#line 2836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value<0))
#line 2836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2838 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_CONST, name, ty, value, 0, alignment);
}
else
#line 2839 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_STRUCT)==1))
#line 2840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_struct_decl();
else
#line 2840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_ENUM)==1))
#line 2841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_enum_decl();
else
#line 2841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2857 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LET)==1))
#line 2857 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2843 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2843 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2843 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2844 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = input_payload();

#line 2845 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2846 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
name = ast_decl_name(name);

#line 2847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COLON)==0))
#line 2847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2848 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ty = ast_type();

#line 2849 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 2849 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2849 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2850 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int alignment = ast_alignment();

#line 2851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((alignment<0))
#line 2851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2852 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_EQUAL)==0))
#line 2852 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2852 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int value = ast_expr();

#line 2854 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value<0))
#line 2854 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2854 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2855 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_SEMI)==0))
#line 2855 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2855 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2856 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_GLOBAL, name, ty, value, 0, alignment);
}
else
#line 2857 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2882 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_FUNC)==1))
#line 2882 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_ID))
#line 2859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2860 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = input_payload();

#line 2861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_pos = (input_pos+1);

#line 2862 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
name = ast_decl_name(name);

#line 2863 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int generic_params = 0;

#line 2864 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int old_scope = ast_generic_scope;

#line 2869 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()==T_LT))
#line 2869 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2866 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
generic_params = ast_generic_params();

#line 2867 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((generic_params<0))
#line 2867 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2867 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2868 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_generic_scope = generic_params;
}
else
#line 2869 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_LPAREN)==0))
#line 2870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2871 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int params = ast_params();

#line 2872 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((params<0))
#line 2872 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2872 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2873 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_RPAREN)==0))
#line 2873 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2873 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2874 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_take(T_COLON)==0))
#line 2874 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2874 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2875 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ret_ty = ast_type();

#line 2876 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ret_ty==0))
#line 2876 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2876 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2877 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int body = ast_stmt();

#line 2878 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((body<0))
#line 2878 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2878 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2879 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_generic_scope = old_scope;

#line 2880 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((generic_params==0))
#line 2880 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_FUNC, body, ret_ty, params, name, node_kind[ret_ty]);
else
#line 2880 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2881 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_GENERIC_FUNC, body, ret_ty, params, name, generic_params);
}
else
#line 2882 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2883 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
}

#line 2903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_flatten_decl_list(int item)
#line 2903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2887 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int head = 0;

#line 2888 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tail = 0;

#line 2889 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = item;

#line 2901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 2901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2891 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int next = node_next[p];

#line 2892 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_next[p] = 0;

#line 2893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int part = p;

#line 2894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[p]==N_LIST))
#line 2894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
part = ast_flatten_decl_list(node_a[p]);
else
#line 2894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((part!=0))
#line 2899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((head==0))
#line 2896 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2896 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
head = part;

#line 2896 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tail = part;
}
else
#line 2897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_next[tail] = part;
}

#line 2898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((node_next[tail]!=0))
#line 2898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tail = node_next[tail];
}
}
else
#line 2899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = next;
}

#line 2902 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return head;
}

#line 2923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_program()
#line 2923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2906 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int items = 0;

#line 2921 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((input_peek()!=T_EOF))
#line 2921 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2908 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = ast_decl();

#line 2909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((item<0))
#line 2909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
else
#line 2909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 2910 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int flat = 0;

#line 2912 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_LIST))
#line 2911 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
flat = ast_flatten_decl_list(node_a[item]);
else
#line 2912 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
flat = item;

#line 2920 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((flat!=0))
#line 2920 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2919 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((items==0))
#line 2914 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = flat;
else
#line 2919 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tail = items;

#line 2917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((node_next[tail]!=0))
#line 2917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 2917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tail = node_next[tail];
}

#line 2918 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_next[tail] = flat;
}
}
else
#line 2920 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 2922 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(N_PROGRAM, items, 0, 0, 0, 0);
}

#line 3022 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void c_source_reset()
#line 3022 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3022 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
c_source_len = 0;
}

#line 3028 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_c_source(int need)
#line 3028 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3024 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<c_source_cap))
#line 3024 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 3024 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3025 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(c_source_cap, need);

#line 3026 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
c_source = grow_ints(c_source, c_source_cap, n);

#line 3027 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
c_source_cap = n;
}

#line 3033 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void c_source_put(int c)
#line 3033 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3030 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_c_source(c_source_len);

#line 3031 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
c_source[c_source_len] = c;

#line 3032 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
c_source_len = (c_source_len+1);
}

#line 3041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_source_file_names(int need)
#line 3041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3036 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<source_file_cap))
#line 3036 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 3036 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3037 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(source_file_cap, need);

#line 3038 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_start = grow_ints(source_file_name_start, source_file_cap, n);

#line 3039 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_len = grow_ints(source_file_name_len, source_file_cap, n);

#line 3040 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_cap = n;
}

#line 3048 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_source_file_text(int need)
#line 3048 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3044 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<source_file_name_text_cap))
#line 3044 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 3044 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3045 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(source_file_name_text_cap, need);

#line 3046 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_text = grow_ints(source_file_name_text, source_file_name_text_cap, n);

#line 3047 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_text_cap = n;
}

#line 3054 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_path_length(char* path)
#line 3054 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3051 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = 0;

#line 3052 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((path[n]!=0))
#line 3052 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3052 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
n = (n+1);
}

#line 3053 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return n;
}

#line 3078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_file_intern(char* path)
#line 3078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3057 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int length = source_path_length(path);

#line 3058 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int id = 1;

#line 3067 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((id<source_file_count))
#line 3067 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3065 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_file_name_len[id]==length))
#line 3065 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3061 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 3062 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int same = 1;

#line 3063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<length))
#line 3063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_file_name_text[(source_file_name_start[id]+i)]!=path[i]))
#line 3063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
same = 0;
else
#line 3063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3064 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((same==1))
#line 3064 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
else
#line 3064 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3065 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3066 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
id = (id+1);
}

#line 3068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
id = source_file_count;

#line 3069 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source_file_names(id);

#line 3070 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source_file_text((source_file_name_text_len+length));

#line 3071 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_start[id] = source_file_name_text_len;

#line 3072 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_len[id] = length;

#line 3073 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int j = 0;

#line 3074 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((j<length))
#line 3074 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3074 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_text[(source_file_name_text_len+j)] = path[j];

#line 3074 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
j = (j+1);
}

#line 3075 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_text_len = (source_file_name_text_len+length);

#line 3076 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_count = (source_file_count+1);

#line 3077 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
}

#line 3123 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_file_intern_include(int*line, int length, int base_id)
#line 3123 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3081 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int q = 0;

#line 3082 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((q<length)&&(line[q]!=34)))
#line 3082 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3082 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
q = (q+1);
}

#line 3083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((q==length))
#line 3083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return base_id;
else
#line 3083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = (q+1);

#line 3085 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int b = a;

#line 3086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((b<length)&&(line[b]!=34)))
#line 3086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
b = (b+1);
}

#line 3087 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((b==length))
#line 3087 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return base_id;
else
#line 3087 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int base_len = 0;

#line 3089 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((base_id>0))
#line 3089 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base_len = source_file_name_len[base_id];
else
#line 3089 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3090 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int prefix_len = 0;

#line 3091 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = base_len;

#line 3092 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int found_slash = 0;

#line 3096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((p>0)&&(found_slash==0)))
#line 3096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3095 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((source_file_name_text[((source_file_name_start[base_id]+p)-1)]==47)||(source_file_name_text[((source_file_name_start[base_id]+p)-1)]==92)))
#line 3094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
prefix_len = p;

#line 3094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
found_slash = 1;
}
else
#line 3095 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = (p-1);
}

#line 3097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int raw_len = (b-a);

#line 3098 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int total = (prefix_len+raw_len);

#line 3099 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int id = 1;

#line 3110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((id<source_file_count))
#line 3110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3108 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_file_name_len[id]==total))
#line 3108 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3102 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 3103 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int same = 1;

#line 3104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<prefix_len))
#line 3104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_file_name_text[(source_file_name_start[id]+i)]!=source_file_name_text[(source_file_name_start[base_id]+i)]))
#line 3104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
same = 0;
else
#line 3104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3105 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = 0;

#line 3106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<raw_len))
#line 3106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_file_name_text[((source_file_name_start[id]+prefix_len)+i)]!=line[(a+i)]))
#line 3106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
same = 0;
else
#line 3106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((same==1))
#line 3107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
else
#line 3107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3108 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3109 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
id = (id+1);
}

#line 3111 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
id = source_file_count;

#line 3112 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source_file_names(id);

#line 3113 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source_file_text((source_file_name_text_len+total));

#line 3114 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_start[id] = source_file_name_text_len;

#line 3115 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_len[id] = total;

#line 3116 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int j = 0;

#line 3117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((j<prefix_len))
#line 3117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_text[(source_file_name_text_len+j)] = source_file_name_text[(source_file_name_start[base_id]+j)];

#line 3117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
j = (j+1);
}

#line 3118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
j = 0;

#line 3119 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((j<raw_len))
#line 3119 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3119 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_text[((source_file_name_text_len+prefix_len)+j)] = line[(a+j)];

#line 3119 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
j = (j+1);
}

#line 3120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_text_len = (source_file_name_text_len+total);

#line 3121 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_count = (source_file_count+1);

#line 3122 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
}

#line 3132 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void source_reset()
#line 3132 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3126 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_len = 0;

#line 3127 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_pos = 0;

#line 3128 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_count = 1;

#line 3129 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_name_text_len = 0;

#line 3130 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_active_file = 0;

#line 3131 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_active_line = 1;
}

#line 3141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void source_put(int c)
#line 3141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3135 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source(source_len);

#line 3136 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source[source_len] = c;

#line 3137 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_file_at[source_len] = source_active_file;

#line 3138 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_line_at[source_len] = source_active_line;

#line 3139 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_len = (source_len+1);

#line 3140 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==10))
#line 3140 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_active_line = (source_active_line+1);
else
#line 3140 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 3149 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int is_space(int c)
#line 3149 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==32))
#line 3144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 3144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==9))
#line 3145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 3145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==10))
#line 3146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 3146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3147 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==13))
#line 3147 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 3147 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3148 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 3155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int is_digit(int c)
#line 3155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c<48))
#line 3152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 3152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3153 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c>57))
#line 3153 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 3153 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3154 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 3166 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int is_alpha(int c)
#line 3166 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3160 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c>64))
#line 3160 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3159 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c<91))
#line 3159 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 3159 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3160 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3163 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c>96))
#line 3163 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3162 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c<123))
#line 3162 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 3162 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3163 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==95))
#line 3164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 3164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3165 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 3171 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int is_alnum(int c)
#line 3171 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((is_alpha(c)==1))
#line 3169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 3169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return is_digit(c);
}

#line 3176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_peek()
#line 3176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3174 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_pos<source_len))
#line 3174 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return source[source_pos];
else
#line 3174 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3175 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 3183 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_take()
#line 3183 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3179 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c = source_peek();

#line 3180 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_pos<source_len))
#line 3180 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_pos = (source_pos+1);
else
#line 3180 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3181 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
current_source_pos = source_pos;

#line 3182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return c;
}

#line 3247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int span_hash(int start, int length)
#line 3247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3239 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 3240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int h = 7;

#line 3245 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<length))
#line 3245 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
h = ((h*31)+source[(start+i)]);

#line 3243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h>1000000))
#line 3243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
h = (h-((h/1000000)*1000000));
else
#line 3243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3244 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3246 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return h;
}

#line 3256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int span_equal(int a, int b, int length)
#line 3256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3250 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 3254 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<length))
#line 3254 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(a+i)]!=source[(b+i)]))
#line 3252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 3252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3255 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 3269 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_lookup(int start, int length, int h)
#line 3269 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3259 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 1;

#line 3267 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<sym_count))
#line 3267 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3265 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((sym_len[i]==length))
#line 3265 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((sym_hash[i]==h))
#line 3264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((span_equal(sym_start[i], start, length)==1))
#line 3263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return i;
else
#line 3263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3265 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3266 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3268 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 3281 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_tag_id()
#line 3281 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3272 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((sym_tag_name!=0))
#line 3272 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return sym_tag_name;
else
#line 3272 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3273 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int start = (source_len+sym_text_len);

#line 3274 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source((start+2));

#line 3275 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source[start] = 116;

#line 3276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source[(start+1)] = 97;

#line 3277 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source[(start+2)] = 103;

#line 3278 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_tag_name = sym_intern(start, 3, L_ID, 0);

#line 3279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_text_len = (sym_text_len+3);

#line 3280 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return sym_tag_name;
}

#line 3296 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_qualified(int ns, int name)
#line 3296 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3284 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ns==0))
#line 3284 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return name;
else
#line 3284 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3285 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int start = (source_len+sym_text_len);

#line 3286 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int out = 0;

#line 3287 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 3288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<sym_len[ns]))
#line 3288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source((start+out));

#line 3288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source[(start+out)] = source[(sym_start[ns]+i)];

#line 3288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
out = (out+1);

#line 3288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3289 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source((start+out));

#line 3289 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source[(start+out)] = 58;

#line 3289 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
out = (out+1);

#line 3290 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source((start+out));

#line 3290 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source[(start+out)] = 58;

#line 3290 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
out = (out+1);

#line 3291 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = 0;

#line 3292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<sym_len[name]))
#line 3292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source((start+out));

#line 3292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source[(start+out)] = source[(sym_start[name]+i)];

#line 3292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
out = (out+1);

#line 3292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3293 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int id = sym_intern(start, out, L_ID, 0);

#line 3294 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_text_len = (sym_text_len+out);

#line 3295 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
}

#line 3301 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_decl_name(int name)
#line 3301 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ast_namespace_scope==0))
#line 3299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return name;
else
#line 3299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3300 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return sym_qualified(ast_namespace_scope, name);
}

#line 3307 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ast_type_name(int name)
#line 3307 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ast_generic_param(name)==1))
#line 3304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return name;
else
#line 3304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ast_namespace_scope==0))
#line 3305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return name;
else
#line 3305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return sym_qualified(ast_namespace_scope, name);
}

#line 3325 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_intern(int start, int length, int kind, int scope)
#line 3325 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3310 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int h = span_hash(start, length);

#line 3311 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int old = sym_lookup(start, length, h);

#line 3312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((old!=0))
#line 3312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return old;
else
#line 3312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3313 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int id = sym_count;

#line 3314 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_sym(id);

#line 3315 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_start[id] = start;

#line 3316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_len[id] = length;

#line 3317 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_hash[id] = h;

#line 3318 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_kind[id] = kind;

#line 3319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_scope[id] = scope;

#line 3320 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_type[id] = 0;

#line 3321 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_elem_kind[id] = 0;

#line 3322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_elem_name[id] = 0;

#line 3323 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_count = (sym_count+1);

#line 3324 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return id;
}

#line 3335 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_bi(int need)
#line 3335 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3328 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<bi_cap))
#line 3328 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 3328 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3329 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(bi_cap, need);

#line 3330 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_name = grow_ints(bi_name, bi_cap, n);

#line 3331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_len = grow_ints(bi_len, bi_cap, n);

#line 3332 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_tc = grow_ints(bi_tc, bi_cap, n);

#line 3333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_flags = grow_ints(bi_flags, bi_cap, n);

#line 3334 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_cap = n;
}

#line 3351 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void bi_register(char* text, int tc_tag, int flags)
#line 3351 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3338 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int len = 0;

#line 3339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((text[len]!=0))
#line 3339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
len = (len+1);
}

#line 3340 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int start = (source_len+sym_text_len);

#line 3341 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 3342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<len))
#line 3342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_source((start+i));

#line 3342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source[(start+i)] = text[i];

#line 3342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3343 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_text_len = (sym_text_len+len);

#line 3344 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int id = sym_intern(start, len, L_STRING, 0);

#line 3345 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_bi(bi_count);

#line 3346 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_name[bi_count] = id;

#line 3347 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_len[bi_count] = len;

#line 3348 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_tc[bi_count] = tc_tag;

#line 3349 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_flags[bi_count] = flags;

#line 3350 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_count = (bi_count+1);
}

#line 3361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bi_lookup(int name)
#line 3361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bi_count==0))
#line 3354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_init();
else
#line 3354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3355 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 3359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<bi_count))
#line 3359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((sym_len[name]==bi_len[i])&&(sym_hash[name]==sym_hash[bi_name[i]])))
#line 3357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return i;
else
#line 3357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3358 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return(0-1);
}

#line 3367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bi_tag(int name)
#line 3367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3364 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = bi_lookup(name);

#line 3365 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((i<0))
#line 3365 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return BI_TC_NONE;
else
#line 3365 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3366 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return bi_tc[i];
}

#line 3374 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bi_has_flag(int name, int flag)
#line 3374 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = bi_lookup(name);

#line 3371 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((i<0))
#line 3371 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 3371 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3372 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((bi_flags[i]&flag)!=0))
#line 3372 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 3372 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3373 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 3453 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void bi_init()
#line 3453 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3377 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("printf", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3378 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("runtime_read_line", BI_TC_READ_LINE, (BI_FLAG_RESERVED+BI_FLAG_OWNED));

#line 3379 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("runtime_read_int", BI_TC_READ_INT, BI_FLAG_RESERVED);

#line 3380 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("runtime_write_string", BI_TC_WRITE_STRING, BI_FLAG_RESERVED);

#line 3381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("runtime_write_line", BI_TC_WRITE_LINE, BI_FLAG_RESERVED);

#line 3382 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("runtime_write_int", BI_TC_WRITE_INT, BI_FLAG_RESERVED);

#line 3383 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("runtime_write_char", BI_TC_WRITE_CHAR, BI_FLAG_RESERVED);

#line 3384 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("runtime_io_status", BI_TC_IO_STATUS, BI_FLAG_RESERVED);

#line 3385 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("memory_alloc", BI_TC_MEM_ALLOC, (BI_FLAG_RESERVED+BI_FLAG_OWNED));

#line 3386 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("memory_alloc_aligned", BI_TC_MEM_ALLOC_ALIGNED, (BI_FLAG_RESERVED+BI_FLAG_OWNED));

#line 3387 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("memory_resize", BI_TC_MEM_RESIZE, (BI_FLAG_RESERVED+BI_FLAG_OWNED));

#line 3388 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("memory_free", BI_TC_MEM_FREE, (BI_FLAG_RESERVED+BI_FLAG_CONSUME));

#line 3389 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("alloc_ints", BI_TC_PTR_INT, (BI_FLAG_RESERVED+BI_FLAG_OWNED));

#line 3390 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("free_ints", BI_TC_VOID, (BI_FLAG_RESERVED+BI_FLAG_CONSUME));

#line 3391 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("grow_ints", BI_TC_PTR_INT, BI_FLAG_RESERVED);

#line 3392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("open_file", BI_TC_PTR_VOID, (BI_FLAG_RESERVED+BI_FLAG_OWNED));

#line 3393 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("read_char", BI_TC_INT, BI_FLAG_RESERVED);

#line 3394 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("close_file", BI_TC_INT, (BI_FLAG_RESERVED+BI_FLAG_CONSUME));

#line 3395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("write_char", BI_TC_INT, BI_FLAG_RESERVED);

#line 3396 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("write_string", BI_TC_INT, BI_FLAG_RESERVED);

#line 3397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("write_int", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("runtime_string_concat", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_track", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3400 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_release", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_memory_alloc", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_memory_resize", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3403 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_memory_free", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3404 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_panic", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3405 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_checked_bytes", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3406 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_find", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3407 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_validate", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3408 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_cleanup", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3409 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_inc_find", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3410 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_inc_add", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3411 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_inc_strdup", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_inc_realpath", BI_TC_STRING, BI_FLAG_RESERVED);

#line 3413 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_inc_join", BI_TC_STRING, BI_FLAG_RESERVED);

#line 3414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_include_line_mode", BI_TC_INT, BI_FLAG_RESERVED);

#line 3415 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_include_close", BI_TC_VOID, BI_FLAG_RESERVED);

#line 3416 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_include_open_root", BI_TC_PTR_INT, (BI_FLAG_RESERVED+BI_FLAG_OWNED));

#line 3417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_include_open_line", BI_TC_PTR_INT, (BI_FLAG_RESERVED+BI_FLAG_OWNED));

#line 3418 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_include_last_status", BI_TC_INT, BI_FLAG_RESERVED);

#line 3419 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_include_reset_session", BI_TC_VOID, BI_FLAG_RESERVED);

#line 3420 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_atomic_make", BI_TC_ATOMIC_MAKE, (BI_FLAG_RESERVED+BI_FLAG_OWNED));

#line 3421 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_atomic_load", BI_TC_ATOMIC_LOAD, BI_FLAG_RESERVED);

#line 3422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_atomic_store", BI_TC_ATOMIC_STORE, BI_FLAG_RESERVED);

#line 3423 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_atomic_fetch_add", BI_TC_ATOMIC_FETCH_ADD, BI_FLAG_RESERVED);

#line 3424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_atomic_compare_exchange", BI_TC_ATOMIC_CAS, BI_FLAG_RESERVED);

#line 3425 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_atomic_free", BI_TC_ATOMIC_FREE, BI_FLAG_RESERVED);

#line 3426 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_channel_make", BI_TC_CHANNEL_MAKE, (BI_FLAG_RESERVED+BI_FLAG_OWNED));

#line 3427 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_channel_send", BI_TC_CHANNEL_SEND, BI_FLAG_RESERVED);

#line 3428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_channel_recv", BI_TC_CHANNEL_RECV, BI_FLAG_RESERVED);

#line 3429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_channel_close", BI_TC_CHANNEL_CLOSE, BI_FLAG_RESERVED);

#line 3430 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_channel_free", BI_TC_CHANNEL_FREE, BI_FLAG_RESERVED);

#line 3431 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_thread_spawn", BI_TC_THREAD_SPAWN, (BI_FLAG_RESERVED+BI_FLAG_OWNED));

#line 3432 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_thread_join", BI_TC_THREAD_JOIN, BI_FLAG_RESERVED);

#line 3433 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("basalt_thread_yield", BI_TC_THREAD_YIELD, BI_FLAG_RESERVED);

#line 3434 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("malloc", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3435 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("calloc", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3436 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("realloc", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3437 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("free", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3438 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("memcpy", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3439 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("memset", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3440 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("strlen", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3441 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("strrchr", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3442 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("fopen", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3443 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("fclose", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3444 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("fgetc", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3445 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("fputc", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3446 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("fputs", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3447 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("fprintf", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3448 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("exit", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3449 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("atexit", BI_TC_NONE, BI_FLAG_RESERVED);

#line 3450 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("len", BI_TC_NONE, BI_FLAG_DYNFIELD);

#line 3451 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("cap", BI_TC_NONE, BI_FLAG_DYNFIELD);

#line 3452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bi_register("main", BI_TC_NONE, BI_FLAG_MAIN);
}

#line 3563 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int word_code(int start, int length)
#line 3563 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3456 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int h = span_hash(start, length);

#line 3462 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==2))
#line 3462 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3458 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==10084))
#line 3458 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_IF;
else
#line 3458 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3459 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==9999))
#line 3459 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_FN;
else
#line 3459 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3460 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((source[start]==117)&&(source[(start+1)]==56)))
#line 3460 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TU8;
else
#line 3460 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3461 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((source[start]==105)&&(source[(start+1)]==56)))
#line 3461 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TI8;
else
#line 3461 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3462 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3475 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==3))
#line 3475 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==315572))
#line 3464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_LET;
else
#line 3464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3465 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==312968))
#line 3465 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TINT;
else
#line 3465 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3466 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==310114))
#line 3466 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_FOR;
else
#line 3466 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((source[start]==117)&&(source[(start+1)]==49))&&(source[(start+2)]==54)))
#line 3467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TU16;
else
#line 3467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3468 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((source[start]==102)&&(source[(start+1)]==51))&&(source[(start+2)]==50)))
#line 3468 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TFLOAT;
else
#line 3468 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((source[start]==102)&&(source[(start+1)]==54))&&(source[(start+2)]==52)))
#line 3469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TDOUBLE;
else
#line 3469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((source[start]==117)&&(source[(start+1)]==51))&&(source[(start+2)]==50)))
#line 3470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TU32;
else
#line 3470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((source[start]==117)&&(source[(start+1)]==54))&&(source[(start+2)]==52)))
#line 3471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TU64;
else
#line 3471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((source[start]==105)&&(source[(start+1)]==49))&&(source[(start+2)]==54)))
#line 3472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TI16;
else
#line 3472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((source[start]==105)&&(source[(start+1)]==51))&&(source[(start+2)]==50)))
#line 3473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TI32;
else
#line 3473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3474 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((source[start]==105)&&(source[(start+1)]==54))&&(source[(start+2)]==52)))
#line 3474 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TI64;
else
#line 3474 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3475 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==4))
#line 3484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3477 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==619275))
#line 3477 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_FUNC;
else
#line 3477 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3478 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==580992))
#line 3478 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_ELSE;
else
#line 3478 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3479 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==582984))
#line 3479 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_ENUM;
else
#line 3479 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==33685))
#line 3480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TRUE;
else
#line 3480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==494385))
#line 3481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TBOOL;
else
#line 3481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==90011))
#line 3482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TVOID;
else
#line 3482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==23588))
#line 3483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_THEN;
else
#line 3483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3492 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==5))
#line 3492 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==339014))
#line 3486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_PRINT;
else
#line 3486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3487 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==505674))
#line 3487 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_WHILE;
else
#line 3487 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3488 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==600380))
#line 3488 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_FALSE;
else
#line 3488 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3489 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==405464))
#line 3489 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_BREAK;
else
#line 3489 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3490 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((source[start]==100)&&(source[(start+1)]==101))&&(source[(start+2)]==102))&&(source[(start+3)]==101))&&(source[(start+4)]==114)))
#line 3490 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_DEFER;
else
#line 3490 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((source[start]==109)&&(source[(start+1)]==97))&&(source[(start+2)]==116))&&(source[(start+3)]==99))&&(source[(start+4)]==104)))
#line 3491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_MATCH;
else
#line 3491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3492 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3509 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==8))
#line 3509 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[start]==99))
#line 3508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+1)]==111))
#line 3507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+2)]==110))
#line 3506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+3)]==116))
#line 3505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+4)]==105))
#line 3504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3503 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+5)]==110))
#line 3503 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3502 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+6)]==117))
#line 3502 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+7)]==101))
#line 3501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_CONTINUE;
else
#line 3501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3502 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3503 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3509 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3514 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==4))
#line 3514 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((source[start]==110)&&(source[(start+1)]==117))&&(source[(start+2)]==108))&&(source[(start+3)]==108)))
#line 3511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_NULL;
else
#line 3511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3512 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((source[start]==99)&&(source[(start+1)]==104))&&(source[(start+2)]==97))&&(source[(start+3)]==114)))
#line 3512 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TCHAR;
else
#line 3512 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((source[start]==108)&&(source[(start+1)]==111))&&(source[(start+2)]==110))&&(source[(start+3)]==103)))
#line 3513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TLONG;
else
#line 3513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3514 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==5))
#line 3518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3516 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((source[start]==99)&&(source[(start+1)]==111))&&(source[(start+2)]==110))&&(source[(start+3)]==115))&&(source[(start+4)]==116)))
#line 3516 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_CONST;
else
#line 3516 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3517 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((source[start]==97)&&(source[(start+1)]==114))&&(source[(start+2)]==114))&&(source[(start+3)]==97))&&(source[(start+4)]==121)))
#line 3517 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_ARRAY;
else
#line 3517 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==6))
#line 3554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((h==448999))
#line 3520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_EXTERN;
else
#line 3520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3531 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[start]==114))
#line 3531 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+1)]==101))
#line 3530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3529 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+2)]==116))
#line 3529 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3528 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+3)]==117))
#line 3528 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3527 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+4)]==114))
#line 3527 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+5)]==110))
#line 3526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_RETURN;
else
#line 3526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3527 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3528 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3529 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3531 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3542 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[start]==115))
#line 3542 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3541 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+1)]==116))
#line 3541 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+2)]==114))
#line 3540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+3)]==117))
#line 3539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3538 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+4)]==99))
#line 3538 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3537 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+5)]==116))
#line 3537 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_STRUCT;
else
#line 3537 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3538 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3541 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3542 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3553 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[start]==115))
#line 3553 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+1)]==116))
#line 3552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+2)]==114))
#line 3551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+3)]==105))
#line 3550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+4)]==110))
#line 3549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+5)]==103))
#line 3548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TSTRING;
else
#line 3548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3553 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3558 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==5))
#line 3558 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((source[start]==102)&&(source[(start+1)]==108))&&(source[(start+2)]==111))&&(source[(start+3)]==97))&&(source[(start+4)]==116)))
#line 3556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TFLOAT;
else
#line 3556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((source[start]==117)&&(source[(start+1)]==115))&&(source[(start+2)]==105))&&(source[(start+3)]==122))&&(source[(start+4)]==101)))
#line 3557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TUSIZE;
else
#line 3557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3558 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3559 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==7))
#line 3559 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3559 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((((source[start]==97)&&(source[(start+1)]==108))&&(source[(start+2)]==105))&&(source[(start+3)]==103))&&(source[(start+4)]==110))&&(source[(start+5)]==97))&&(source[(start+6)]==115)))
#line 3559 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_ALIGNAS;
else
#line 3559 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3559 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==9))
#line 3560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((((((source[start]==110)&&(source[(start+1)]==97))&&(source[(start+2)]==109))&&(source[(start+3)]==101))&&(source[(start+4)]==115))&&(source[(start+5)]==112))&&(source[(start+6)]==97))&&(source[(start+7)]==99))&&(source[(start+8)]==101)))
#line 3560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_NAMESPACE;
else
#line 3560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3561 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length==6))
#line 3561 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3561 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((((source[start]==100)&&(source[(start+1)]==111))&&(source[(start+2)]==117))&&(source[(start+3)]==98))&&(source[(start+4)]==108))&&(source[(start+5)]==101)))
#line 3561 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_TDOUBLE;
else
#line 3561 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3561 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3562 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return L_ID;
}

#line 3586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void lexer_skip()
#line 3586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3574 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((is_space(source_peek())==1))
#line 3574 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();
}

#line 3585 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==47))
#line 3585 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3584 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((source_pos+1)<source_len))
#line 3584 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3583 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(source_pos+1)]==47))
#line 3583 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((source_peek()!=10))
#line 3581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3579 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_pos>(source_len-1)))
#line 3579 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 3579 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3580 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();
}

#line 3582 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
lexer_skip();
}
else
#line 3583 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3584 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3585 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 3702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int lexer_next()
#line 3702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3589 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
lexer_skip();

#line 3590 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_start = source_pos;

#line 3591 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_length = 0;

#line 3592 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_text = 0;

#line 3593 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c = source_peek();

#line 3598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==0))
#line 3598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3595 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_EOF;

#line 3596 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_value = 0;

#line 3597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tok_kind;
}
else
#line 3598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((is_alpha(c)==1))
#line 3608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((is_alnum(source_peek())==1))
#line 3602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3601 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();
}

#line 3603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_length = (source_pos-tok_start);

#line 3604 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = word_code(tok_start, tok_length);

#line 3606 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tok_kind==L_ID)||(tok_kind==L_ARRAY)))
#line 3605 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_value = sym_intern(tok_start, tok_length, tok_kind, 0);
else
#line 3606 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_value = 0;

#line 3607 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tok_kind;
}
else
#line 3608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==39))
#line 3616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3610 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3611 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int v = source_take();

#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((v==92))
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int e = source_take();

#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e==110))
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
v = 10;
else
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e==116))
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
v = 9;
else
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e==114))
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
v = 13;
else
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e==98))
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
v = 8;
else
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e==102))
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
v = 12;
else
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e==118))
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
v = 11;
else
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e==48))
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
v = 0;
else
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
v = e;
}
else
#line 3612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()!=39))
#line 3613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_EOF;

#line 3613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_value = 0;

#line 3613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tok_kind;
}
else
#line 3613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3614 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_CHAR;

#line 3615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_value = v;

#line 3615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_length = 3;

#line 3615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tok_kind;
}
else
#line 3616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==34))
#line 3635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3618 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3629 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((source_peek()!=34))
#line 3629 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_pos>(source_len-1)))
#line 3624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_EOF;

#line 3622 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_value = 0;

#line 3623 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tok_kind;
}
else
#line 3624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==92))
#line 3628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3626 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()!=0))
#line 3627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();
else
#line 3627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();
}

#line 3630 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3631 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_STRING;

#line 3632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_length = (source_pos-tok_start);

#line 3633 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_value = sym_intern((tok_start+1), (tok_length-2), L_STRING, 0);

#line 3634 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tok_kind;
}
else
#line 3635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((is_digit(c)==1))
#line 3654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3637 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int value = 0;

#line 3638 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int overflow = 0;

#line 3647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((is_digit(source_peek())==1))
#line 3647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3640 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int digit = (source_peek()-48);

#line 3641 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((overflow==0))
#line 3646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3645 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value>214748364))
#line 3643 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
overflow = 1;
else
#line 3645 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((value==214748364)&&(digit>7)))
#line 3644 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
overflow = 1;
else
#line 3645 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
value = ((value*10)+digit);
}
else
#line 3646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==46))
#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((is_digit(source_peek())==1))
#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();
}

#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_FLOAT;

#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_length = (source_pos-tok_start);

#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_text = 0;

#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_value = sym_intern(tok_start, tok_length, L_FLOAT, 0);

#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tok_kind;
}
else
#line 3648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3649 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_INT;

#line 3650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_length = (source_pos-tok_start);

#line 3651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_text = sym_intern(tok_start, tok_length, L_INT, 0);

#line 3652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((overflow==1))
#line 3652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_value = (0-1);
else
#line 3652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_value = value;

#line 3653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tok_kind;
}
else
#line 3654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3656 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_length = 1;

#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==43))
#line 3661 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3658 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3658 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3658 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_PLUS_EQ;
}
else
#line 3660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==43))
#line 3659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_CONCAT;
}
else
#line 3660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_PLUS;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==45))
#line 3662 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3662 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3662 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3662 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3662 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_MINUS_EQ;
}
else
#line 3662 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_MINUS;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==42))
#line 3663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_STAR_EQ;
}
else
#line 3663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_STAR;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==47))
#line 3664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_DIV_EQ;
}
else
#line 3664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_DIV;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==37))
#line 3665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_MOD_EQ;
}
else
#line 3665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_MOD;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==40))
#line 3666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_LPAREN;
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==41))
#line 3667 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_RPAREN;
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==123))
#line 3668 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_LBRACE;
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==125))
#line 3669 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_RBRACE;
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==58))
#line 3670 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3670 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==58))
#line 3670 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3670 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3670 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_SCOPE;
}
else
#line 3670 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_COLON;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==59))
#line 3671 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_SEMI;
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==44))
#line 3672 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_COMMA;
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==91))
#line 3673 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_LBRACK;
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==93))
#line 3674 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_RBRACK;
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==46))
#line 3675 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_DOT;
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==60))
#line 3676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==60))
#line 3676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_SHL_EQ;
}
else
#line 3676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_SHL;
}
else
#line 3676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_LT;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==62))
#line 3677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==62))
#line 3677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_SHR_EQ;
}
else
#line 3677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_SHR;
}
else
#line 3677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_GT;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==124))
#line 3678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==124))
#line 3678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_OR;
}
else
#line 3678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_BITOR_EQ;
}
else
#line 3678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_BITOR;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==94))
#line 3679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_BITXOR_EQ;
}
else
#line 3679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_BITXOR;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==126))
#line 3680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_BITNOT;
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==61))
#line 3685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_EQEQ;
}
else
#line 3684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==62))
#line 3683 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3683 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3683 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_FATARROW;
}
else
#line 3684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_EQ;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==33))
#line 3689 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_NEQ;
}
else
#line 3688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_NEQ;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==38))
#line 3694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==38))
#line 3691 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3691 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3691 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_AND;
}
else
#line 3693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==61))
#line 3692 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3692 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3692 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_AMP_EQ;
}
else
#line 3693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_AMP;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==124))
#line 3698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3697 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_peek()==124))
#line 3696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_take();

#line 3696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_OR;
}
else
#line 3697 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_EOF;
}
else
#line 3699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_kind = L_EOF;

#line 3700 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tok_value = 0;

#line 3701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tok_kind;
}

#line 3744 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void include_process_line(int*line, int length)
#line 3744 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3705 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int mode = 0;

#line 3706 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = 0;

#line 3707 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((p<length)&&((line[p]==32)||(line[p]==9))))
#line 3707 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3707 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = (p+1);
}

#line 3712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((p+7)<length))
#line 3712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((((line[p]==105)&&(line[(p+1)]==110))&&(line[(p+2)]==99))&&(line[(p+3)]==108))&&(line[(p+4)]==117))&&(line[(p+5)]==100))&&(line[(p+6)]==101)))
#line 3711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((line[(p+7)]==32)||(line[(p+7)]==9)))
#line 3710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
mode = 1;
else
#line 3710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3717 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((p+8)<length)&&(mode==0)))
#line 3717 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3716 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((((((line[p]==105)&&(line[(p+1)]==110))&&(line[(p+2)]==99))&&(line[(p+3)]==108))&&(line[(p+4)]==117))&&(line[(p+5)]==100))&&(line[(p+6)]==101))&&(line[(p+7)]==99)))
#line 3716 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3715 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((line[(p+8)]==32)||(line[(p+8)]==9)))
#line 3715 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
mode = 2;
else
#line 3715 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3716 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3717 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((mode==0))
#line 3722 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3719 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 3720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<length))
#line 3720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_put(line[i]);

#line 3720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3721 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_put(10);
}
else
#line 3743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3723 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int child_file = source_file_intern_include(line, length, source_active_file);

#line 3724 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*child = basalt_include_open_line(line, length, mode);

#line 3742 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((child==0))
#line 3727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((basalt_include_last_status()==1))
#line 3726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
include_ok = 0;
else
#line 3726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 3742 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3739 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((mode==1))
#line 3736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3729 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int parent_file = source_active_file;

#line 3730 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int parent_line = source_active_line;

#line 3731 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_active_file = child_file;

#line 3732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_active_line = 1;

#line 3733 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
include_expand_handle(child);

#line 3734 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_active_file = parent_file;

#line 3735 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_active_line = parent_line;
}
else
#line 3739 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c = read_char(child);

#line 3738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((c!=(0-1)))
#line 3738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
c_source_put(c);

#line 3738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
c = read_char(child);
}
}

#line 3740 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
close_file(child);

#line 3741 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
basalt_include_close();
}
}
}

#line 3761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void include_expand_handle(int*handle)
#line 3761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3747 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*line = alloc_ints(include_line_cap);

#line 3748 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int length = 0;

#line 3749 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int line_number = 1;

#line 3750 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c = read_char(handle);

#line 3758 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((c!=(0-1)))
#line 3758 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3756 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==10))
#line 3752 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3752 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
include_process_line(line, length);

#line 3752 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
length = 0;

#line 3752 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
line_number = (line_number+1);

#line 3752 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_active_line = line_number;
}
else
#line 3756 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3755 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length<include_line_cap))
#line 3754 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3754 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
line[length] = c;

#line 3754 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
length = (length+1);
}
else
#line 3755 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
include_ok = 0;
}

#line 3757 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
c = read_char(handle);
}

#line 3759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((length>0))
#line 3759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
include_process_line(line, length);
else
#line 3759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 3772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void load_source_file(char* path)
#line 3772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3764 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_reset();

#line 3765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*handle = open_file(path, "r");

#line 3766 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c = read_char(handle);

#line 3770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((c!=(0-1)))
#line 3770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3768 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_put(c);

#line 3769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
c = read_char(handle);
}

#line 3771 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
close_file(handle);
}

#line 3865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int map_token(int k)
#line 3865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3776 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_EOF))
#line 3776 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_EOF;
else
#line 3776 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3777 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_ID))
#line 3777 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_ID;
else
#line 3777 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_INT))
#line 3778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_INT;
else
#line 3778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3779 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_STRING))
#line 3779 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_STRING;
else
#line 3779 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3780 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_FUNC))
#line 3780 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_FUNC;
else
#line 3780 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3781 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_EXTERN))
#line 3781 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_EXTERN;
else
#line 3781 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3782 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_LET))
#line 3782 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_LET;
else
#line 3782 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3783 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_PRINT))
#line 3783 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_PRINT;
else
#line 3783 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3784 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_RETURN))
#line 3784 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_RETURN;
else
#line 3784 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_DEFER))
#line 3785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_DEFER;
else
#line 3785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_MATCH))
#line 3786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_MATCH;
else
#line 3786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_FATARROW))
#line 3787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_FATARROW;
else
#line 3787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_IF))
#line 3788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_IF;
else
#line 3788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3789 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_ELSE))
#line 3789 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_ELSE;
else
#line 3789 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3790 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_WHILE))
#line 3790 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_WHILE;
else
#line 3790 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_FOR))
#line 3791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_FOR;
else
#line 3791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3792 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_STRUCT))
#line 3792 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_STRUCT;
else
#line 3792 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3793 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_ENUM))
#line 3793 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_ENUM;
else
#line 3793 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_BREAK))
#line 3794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_BREAK;
else
#line 3794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3795 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_CONTINUE))
#line 3795 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_CONTINUE;
else
#line 3795 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3796 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TRUE))
#line 3796 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TRUE;
else
#line 3796 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3797 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_FALSE))
#line 3797 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_FALSE;
else
#line 3797 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3798 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TINT))
#line 3798 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TINT;
else
#line 3798 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TBOOL))
#line 3799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TBOOL;
else
#line 3799 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3800 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TSTRING))
#line 3800 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TSTRING;
else
#line 3800 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TVOID))
#line 3801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TVOID;
else
#line 3801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3802 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_THEN))
#line 3802 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_THEN;
else
#line 3802 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_PLUS))
#line 3803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_PLUS;
else
#line 3803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3804 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_MINUS))
#line 3804 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_MINUS;
else
#line 3804 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3805 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_STAR))
#line 3805 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_STAR;
else
#line 3805 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_DIV))
#line 3806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_DIVIDE;
else
#line 3806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_MOD))
#line 3807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_MOD;
else
#line 3807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TLONG))
#line 3808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TLONG;
else
#line 3808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3809 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_ALIGNAS))
#line 3809 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_ALIGNAS;
else
#line 3809 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TU8))
#line 3810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TU8;
else
#line 3810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TU16))
#line 3811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TU16;
else
#line 3811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3812 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TU32))
#line 3812 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TU32;
else
#line 3812 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TU64))
#line 3813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TU64;
else
#line 3813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TI8))
#line 3814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TI8;
else
#line 3814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3815 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TI16))
#line 3815 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TI16;
else
#line 3815 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3816 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TI32))
#line 3816 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TI32;
else
#line 3816 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3817 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TI64))
#line 3817 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TI64;
else
#line 3817 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3818 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TUSIZE))
#line 3818 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TUSIZE;
else
#line 3818 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_PLUS_EQ))
#line 3819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_PLUS_EQ;
else
#line 3819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_MINUS_EQ))
#line 3820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_MINUS_EQ;
else
#line 3820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3821 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_STAR_EQ))
#line 3821 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_STAR_EQ;
else
#line 3821 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_DIV_EQ))
#line 3822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_DIV_EQ;
else
#line 3822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_MOD_EQ))
#line 3823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_MOD_EQ;
else
#line 3823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3824 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_AMP_EQ))
#line 3824 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_AMP_EQ;
else
#line 3824 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_BITOR_EQ))
#line 3825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_BITOR_EQ;
else
#line 3825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_BITXOR_EQ))
#line 3826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_BITXOR_EQ;
else
#line 3826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3827 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_SHL_EQ))
#line 3827 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_SHL_EQ;
else
#line 3827 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_SHR_EQ))
#line 3828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_SHR_EQ;
else
#line 3828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_CONCAT))
#line 3829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_CONCAT;
else
#line 3829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3830 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_AND))
#line 3830 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_AND_AND;
else
#line 3830 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3831 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_OR))
#line 3831 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_OR_OR;
else
#line 3831 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_EQ))
#line 3832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_EQUAL;
else
#line 3832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_EQEQ))
#line 3833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_EQEQ;
else
#line 3833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3834 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_NEQ))
#line 3834 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_NEQ;
else
#line 3834 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_LT))
#line 3835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_LT;
else
#line 3835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_GT))
#line 3836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_GT;
else
#line 3836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_COLON))
#line 3837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_COLON;
else
#line 3837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3838 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_LPAREN))
#line 3838 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_LPAREN;
else
#line 3838 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3839 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_RPAREN))
#line 3839 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_RPAREN;
else
#line 3839 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_LBRACE))
#line 3840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_LBRACE;
else
#line 3840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_RBRACE))
#line 3841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_RBRACE;
else
#line 3841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3842 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_SEMI))
#line 3842 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_SEMI;
else
#line 3842 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3843 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_COMMA))
#line 3843 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_COMMA;
else
#line 3843 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3844 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_AMP))
#line 3844 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_AMP;
else
#line 3844 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3845 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_LBRACK))
#line 3845 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_LBRACK;
else
#line 3845 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3846 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_RBRACK))
#line 3846 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_RBRACK;
else
#line 3846 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_DOT))
#line 3847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_DOT;
else
#line 3847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3848 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_CHAR))
#line 3848 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_CHAR;
else
#line 3848 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3849 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_NULL))
#line 3849 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_NULL;
else
#line 3849 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3850 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_CONST))
#line 3850 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_CONST;
else
#line 3850 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TCHAR))
#line 3851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TCHAR;
else
#line 3851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3852 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_FLOAT))
#line 3852 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_FLOAT;
else
#line 3852 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TFLOAT))
#line 3853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_FLOAT;
else
#line 3853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3854 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_TDOUBLE))
#line 3854 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_TDOUBLE;
else
#line 3854 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3855 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_BITOR))
#line 3855 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_BITOR;
else
#line 3855 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3856 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_BITXOR))
#line 3856 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_BITXOR;
else
#line 3856 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3857 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_BITNOT))
#line 3857 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_BITNOT;
else
#line 3857 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_SHL))
#line 3858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_SHL;
else
#line 3858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_SHR))
#line 3859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_SHR;
else
#line 3859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3860 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_FN))
#line 3860 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_FN;
else
#line 3860 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_ARRAY))
#line 3861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_ARRAY;
else
#line 3861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3862 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_NAMESPACE))
#line 3862 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_NAMESPACE;
else
#line 3862 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3863 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==L_SCOPE))
#line 3863 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_SCOPE;
else
#line 3863 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3864 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return T_EOF;
}

#line 3901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void load_tokens_from_file(char* path)
#line 3901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3868 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_reset();

#line 3869 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
basalt_include_reset_session();

#line 3870 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_reset();

#line 3871 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_text_len = 0;

#line 3872 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_count = 1;

#line 3873 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_namespace_scope = 0;

#line 3874 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
c_source_reset();

#line 3875 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
include_ok = 1;

#line 3876 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*handle = basalt_include_open_root(path);

#line 3884 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((handle==0))
#line 3877 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
include_ok = 0;
else
#line 3884 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3879 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_active_file = source_file_intern(path);

#line 3880 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_active_line = 1;

#line 3881 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
include_expand_handle(handle);

#line 3882 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
close_file(handle);

#line 3883 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
basalt_include_close();
}

#line 3885 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int k = lexer_next();

#line 3899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((k!=L_EOF))
#line 3899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3896 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((debug_tokens==1))
#line 3896 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3888 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "token.kind");

#line 3889 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%d\n", map_token(k));

#line 3890 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "token.start");

#line 3891 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%d\n", tok_start);

#line 3892 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "token.length");

#line 3893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%d\n", tok_length);

#line 3894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "token.value");

#line 3895 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%d\n", tok_value);
}
else
#line 3896 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_put(map_token(k), tok_value, tok_text, tok_start);

#line 3898 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
k = lexer_next();
}

#line 3900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
input_put(T_EOF, 0, 0, source_pos);
}

#line 3963 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_tc_vars(int need)
#line 3963 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3950 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<tc_var_cap))
#line 3950 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 3950 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3951 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(tc_var_cap, need);

#line 3952 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_name = grow_ints(tc_var_name, tc_var_cap, n);

#line 3953 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_kind = grow_ints(tc_var_kind, tc_var_cap, n);

#line 3954 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_named = grow_ints(tc_var_named, tc_var_cap, n);

#line 3955 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_elem_kind = grow_ints(tc_var_elem_kind, tc_var_cap, n);

#line 3956 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_elem_name = grow_ints(tc_var_elem_name, tc_var_cap, n);

#line 3957 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_type = grow_ints(tc_var_type, tc_var_cap, n);

#line 3958 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_owned = grow_ints(tc_var_owned, tc_var_cap, n);

#line 3959 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_moved = grow_ints(tc_var_moved, tc_var_cap, n);

#line 3960 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_borrow_count = grow_ints(tc_var_borrow_count, tc_var_cap, n);

#line 3961 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_borrow_source = grow_ints(tc_var_borrow_source, tc_var_cap, n);

#line 3962 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_cap = n;
}

#line 3970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_tc_scopes(int need)
#line 3970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3966 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<tc_scope_cap))
#line 3966 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 3966 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3967 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(tc_scope_cap, need);

#line 3968 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_scope_start = grow_ints(tc_scope_start, tc_scope_cap, n);

#line 3969 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_scope_cap = n;
}

#line 3976 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_enter_scope()
#line 3976 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3973 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_tc_scopes(tc_scope_count);

#line 3974 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_scope_start[tc_scope_count] = tc_var_count;

#line 3975 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_scope_count = (tc_scope_count+1);
}

#line 3992 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_leave_scope()
#line 3992 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3979 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int begin = 0;

#line 3980 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 3981 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int source_index = 0;

#line 3982 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_scope_count==0))
#line 3982 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 3982 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3983 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_scope_count = (tc_scope_count-1);

#line 3984 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
begin = tc_scope_start[tc_scope_count];

#line 3985 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = begin;

#line 3990 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<tc_var_count))
#line 3990 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3987 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
source_index = tc_var_borrow_source[i];

#line 3988 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_index<0))
#line 3988 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 3988 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((source_index<begin)&&(tc_var_borrow_count[source_index]>0)))
#line 3988 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_borrow_count[source_index] = (tc_var_borrow_count[source_index]-1);
else
#line 3988 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3989 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 3991 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_count = begin;
}

#line 3999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_tc_path(int need)
#line 3999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 3995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<tc_path_cap))
#line 3995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 3995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 3996 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(tc_path_cap, need);

#line 3997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_path_name = grow_ints(tc_path_name, tc_path_cap, n);

#line 3998 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_path_cap = n;
}

#line 4007 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_fail(int code)
#line 4007 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4006 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_ok==1))
#line 4006 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4003 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_ok = 0;

#line 4004 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_error_code = code;

#line 4005 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_error_pos = current_source_pos;
}
else
#line 4006 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 4015 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void ensure_tc_bindings(int need)
#line 4015 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((need<tc_bind_cap))
#line 4010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 4010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4011 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = next_capacity(tc_bind_cap, need);

#line 4012 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_bind_name = grow_ints(tc_bind_name, tc_bind_cap, n);

#line 4013 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_bind_type = grow_ints(tc_bind_type, tc_bind_cap, n);

#line 4014 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_bind_cap = n;
}

#line 4017 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_bind_clear()
#line 4017 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4017 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_bind_count = 0;
}

#line 4023 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_bind_find(int name)
#line 4023 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4020 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 4021 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<tc_bind_count))
#line 4021 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4021 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_bind_name[i]==name))
#line 4021 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_bind_type[i];
else
#line 4021 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4021 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 4022 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4036 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_bind_add(int name, int ty)
#line 4036 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4026 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int old = tc_bind_find(name);

#line 4030 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((old!=0))
#line 4030 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4028 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_type_equal(old, ty)==0))
#line 4028 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4028 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);

#line 4028 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 4028 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4029 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4030 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4031 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_tc_bindings(tc_bind_count);

#line 4032 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_bind_name[tc_bind_count] = name;

#line 4033 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_bind_type[tc_bind_count] = ty;

#line 4034 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_bind_count = (tc_bind_count+1);

#line 4035 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 4043 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_is_integer_kind(int kind)
#line 4043 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4039 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((kind==TY_INT)||(kind==TY_BOOL))||(kind==TY_CHAR))||(kind==TY_LONG))||(kind==TY_LLONG)))
#line 4039 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4039 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4040 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((kind==TY_U8)||(kind==TY_U16))||(kind==TY_U32))||(kind==TY_U64)))
#line 4040 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4040 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((kind==TY_I8)||(kind==TY_I16))||(kind==TY_I32))||(kind==TY_I64))||(kind==TY_USIZE)))
#line 4041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4042 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4048 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_is_numeric_kind(int kind)
#line 4048 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4045 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_is_integer_kind(kind)==1))
#line 4045 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4045 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4046 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((kind==TY_FLOAT)||(kind==TY_DOUBLE)))
#line 4046 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4046 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4047 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4053 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_is_fixed_integer_kind(int kind)
#line 4053 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4050 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((kind==TY_U8)||(kind==TY_U16))||(kind==TY_U32))||(kind==TY_U64)))
#line 4050 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4050 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4051 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((kind==TY_I8)||(kind==TY_I16))||(kind==TY_I32))||(kind==TY_I64))||(kind==TY_USIZE)))
#line 4051 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4051 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4052 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4057 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_is_legacy_integer_kind(int kind)
#line 4057 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4055 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((kind==TY_INT)||(kind==TY_BOOL))||(kind==TY_CHAR))||(kind==TY_LONG))||(kind==TY_LLONG)))
#line 4055 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4055 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4056 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4073 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_decimal_le(int raw, char* limit)
#line 4073 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((raw==0))
#line 4060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4061 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = sym_len[raw];

#line 4062 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int m = 0;

#line 4063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((limit[m]!=0))
#line 4063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
m = (m+1);
}

#line 4064 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((n<m))
#line 4064 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4064 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4065 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((n>m))
#line 4065 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4065 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4066 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 4071 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<n))
#line 4071 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(sym_start[raw]+i)]<limit[i]))
#line 4068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4069 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(sym_start[raw]+i)]>limit[i]))
#line 4069 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4069 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4070 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 4072 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 4088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_literal_fits(int id, int target_kind)
#line 4088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4076 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((id==0)||(node_kind[id]!=N_INT))||(node_aux[id]==0)))
#line 4076 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4076 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4077 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((target_kind==TY_INT))
#line 4077 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_decimal_le(node_aux[id], "2147483647");
else
#line 4077 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((target_kind==TY_U8))
#line 4078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_decimal_le(node_aux[id], "255");
else
#line 4078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4079 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((target_kind==TY_U16))
#line 4079 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_decimal_le(node_aux[id], "65535");
else
#line 4079 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4080 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((target_kind==TY_U32))
#line 4080 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_decimal_le(node_aux[id], "4294967295");
else
#line 4080 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4081 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((target_kind==TY_U64))
#line 4081 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_decimal_le(node_aux[id], "18446744073709551615");
else
#line 4081 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4082 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((target_kind==TY_I8))
#line 4082 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_decimal_le(node_aux[id], "127");
else
#line 4082 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((target_kind==TY_I16))
#line 4083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_decimal_le(node_aux[id], "32767");
else
#line 4083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((target_kind==TY_I32))
#line 4084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_decimal_le(node_aux[id], "2147483647");
else
#line 4084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4085 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((target_kind==TY_I64))
#line 4085 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_decimal_le(node_aux[id], "9223372036854775807");
else
#line 4085 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((target_kind==TY_USIZE))
#line 4086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_decimal_le(node_aux[id], "18446744073709551615");
else
#line 4086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4087 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 4122 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_type_equal(int a, int b)
#line 4122 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4091 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==0)||(b==0)))
#line 4091 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4091 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4092 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ak = node_kind[a];

#line 4092 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bk = node_kind[b];

#line 4093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ak==TY_PARAM)||(bk==TY_PARAM)))
#line 4093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ak==bk)&&(node_value[a]==node_value[b])))
#line 4093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 4093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ak!=bk))
#line 4094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4095 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((((((((ak==TY_INT)||(ak==TY_BOOL))||(ak==TY_STRING))||(ak==TY_CHAR))||(ak==TY_FLOAT))||(ak==TY_DOUBLE))||(ak==TY_LONG))||(ak==TY_LLONG))||(ak==TY_VOID))||(tc_is_fixed_integer_kind(ak)==1)))
#line 4095 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4095 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ak==TY_NAMED))
#line 4096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_value[a]==node_value[b]))
#line 4096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 4096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ak==TY_VARIANT))
#line 4097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_value[a]==node_value[b])&&(node_aux[a]==node_aux[b])))
#line 4097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 4097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4098 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ak==TY_PTR))
#line 4098 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_type_equal(node_a[a], node_a[b]);
else
#line 4098 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4099 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ak==TY_ARRAY))
#line 4099 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return((node_value[a]==node_value[b])&&tc_type_equal(node_a[a], node_a[b]));
else
#line 4099 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4100 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ak==TY_DYN_ARRAY))
#line 4100 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_type_equal(node_a[a], node_a[b]);
else
#line 4100 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ak==TY_GENERIC))
#line 4107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4102 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_value[a]!=node_value[b]))
#line 4102 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4102 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4103 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int x = node_a[a];

#line 4103 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int y = node_a[b];

#line 4104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((x!=0)&&(y!=0)))
#line 4104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_type_equal(x, y)==0))
#line 4104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
x = node_next[x];

#line 4104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
y = node_next[y];
}

#line 4105 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((x!=0)||(y!=0)))
#line 4105 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4105 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4113 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ak==TY_TUPLE))
#line 4113 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4109 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int x = node_a[a];

#line 4109 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int y = node_a[b];

#line 4110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((x!=0)&&(y!=0)))
#line 4110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_type_equal(x, y)==0))
#line 4110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
x = node_next[x];

#line 4110 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
y = node_next[y];
}

#line 4111 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((x!=0)||(y!=0)))
#line 4111 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4111 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4112 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4113 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ak==TY_FUN))
#line 4120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4115 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_type_equal(node_b[a], node_b[b])==0))
#line 4115 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4115 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4116 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int x = node_a[a];

#line 4116 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int y = node_a[b];

#line 4117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((x!=0)&&(y!=0)))
#line 4117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_type_equal(x, y)==0))
#line 4117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
x = node_next[x];

#line 4117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
y = node_next[y];
}

#line 4118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((x!=0)||(y!=0)))
#line 4118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4119 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4121 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 4137 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_signature_type(int entry)
#line 4137 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4125 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((entry==0))
#line 4125 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4125 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4126 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int args = 0;

#line 4127 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_c[entry];

#line 4133 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 4133 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4129 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int src = node_b[p];

#line 4130 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int q = ast_node(node_kind[src], node_a[src], node_b[src], node_c[src], node_value[src], node_aux[src]);

#line 4131 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((args==0))
#line 4131 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = q;
else
#line 4131 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = ast_link(args, q);

#line 4132 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 4134 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ret = node_b[entry];

#line 4135 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ret!=0))
#line 4135 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ret = ast_node(node_kind[ret], node_a[ret], node_b[ret], node_c[ret], node_value[ret], node_aux[ret]);
else
#line 4135 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4136 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_FUN, args, ret, 0, 0, 0);
}

#line 4147 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_type_node_from_summary(int kind, int name, int elem_kind, int elem_name)
#line 4147 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4140 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_GENERIC))
#line 4140 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4140 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((name!=0))
#line 4140 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return name;
else
#line 4140 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4140 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_TUPLE))
#line 4141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((name!=0))
#line 4141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return name;
else
#line 4141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4141 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4142 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_NAMED))
#line 4142 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_NAMED, 0, 0, 0, name, 0);
else
#line 4142 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_VARIANT))
#line 4143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_VARIANT, 0, 0, 0, name, elem_name);
else
#line 4143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_PTR))
#line 4144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_PTR, tc_type_node_from_summary(elem_kind, elem_name, 0, 0), 0, 0, 0, 0);
else
#line 4144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_DYN_ARRAY))
#line 4145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_DYN_ARRAY, tc_type_node_from_summary(elem_kind, elem_name, 0, 0), 0, 0, 0, 0);
else
#line 4145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(kind, 0, 0, 0, 0, 0);
}

#line 4159 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_generic_moves_array(int fun_node)
#line 4159 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((fun_node==0))
#line 4150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4151 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ret = node_b[fun_node];

#line 4152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ret!=0)&&(node_kind[ret]==TY_DYN_ARRAY)))
#line 4152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4152 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4153 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int name = node_value[fun_node];

#line 4154 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((name==0)||(sym_len[name]<4)))
#line 4154 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4154 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int s = sym_start[name];

#line 4156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = sym_len[name];

#line 4157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((source[((s+n)-4)]==102)&&(source[((s+n)-3)]==114))&&(source[((s+n)-2)]==101))&&(source[((s+n)-1)]==101)))
#line 4157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4158 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_mark_float_expr(int id, int expected_kind)
#line 4170 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4162 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((id==0))
#line 4162 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 4162 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_FLOAT))
#line 4166 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4165 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((expected_kind==TY_FLOAT))
#line 4164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux[id] = TY_FLOAT;
else
#line 4165 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((expected_kind==TY_DOUBLE))
#line 4165 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux[id] = 0;
else
#line 4165 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((expected_kind==TY_FLOAT)&&(node_kind[id]==N_BINOP)))
#line 4169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(node_a[id], TY_FLOAT);

#line 4168 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(node_b[id], TY_FLOAT);
}
else
#line 4169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 4178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_match_generic_call_arg(int formal, int actual, int expr)
#line 4178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((formal!=0)&&(expr!=0))&&(node_kind[formal]==TY_PARAM)))
#line 4176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4174 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bound = tc_bind_find(node_value[formal]);

#line 4175 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[expr]==N_INT)&&(bound!=0))&&(tc_is_integer_kind(node_kind[bound])==1)))
#line 4175 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 4175 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4177 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_match_generic(formal, actual);
}

#line 4209 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_match_generic(int formal, int actual)
#line 4209 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4181 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((formal==0)||(actual==0)))
#line 4181 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4181 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);

#line 4181 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4181 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[formal]==TY_PARAM))
#line 4182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_bind_add(node_value[formal], actual)==0))
#line 4182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 4182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4189 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[formal]==TY_GENERIC))
#line 4189 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4184 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[actual]!=TY_GENERIC)||(node_value[formal]!=node_value[actual])))
#line 4184 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4184 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);

#line 4184 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4184 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[formal];

#line 4185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = node_a[actual];

#line 4186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((f!=0)&&(a!=0)))
#line 4186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_match_generic(f, a);

#line 4186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];

#line 4186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}

#line 4187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((f!=0)||(a!=0)))
#line 4187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);
else
#line 4187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4188 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4189 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4196 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[formal]==TY_TUPLE))
#line 4196 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4191 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[actual]!=TY_TUPLE))
#line 4191 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4191 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);

#line 4191 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4191 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4192 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[formal];

#line 4192 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = node_a[actual];

#line 4193 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((f!=0)&&(a!=0)))
#line 4193 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4193 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_match_generic(f, a);

#line 4193 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];

#line 4193 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}

#line 4194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((f!=0)||(a!=0)))
#line 4194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);
else
#line 4194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4195 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4196 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[formal]==TY_FUN)&&(node_kind[actual]==TY_FUN)))
#line 4203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4198 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int fp = node_a[formal];

#line 4198 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ap = node_a[actual];

#line 4199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((fp!=0)&&(ap!=0)))
#line 4199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_match_generic(fp, ap);

#line 4199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
fp = node_next[fp];

#line 4199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ap = node_next[ap];
}

#line 4200 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((fp!=0)||(ap!=0)))
#line 4200 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4200 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4200 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4200 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4201 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_match_generic(node_b[formal], node_b[actual]);

#line 4202 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4204 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[formal]==TY_PTR)&&(node_kind[actual]==TY_PTR)))
#line 4204 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4204 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_match_generic(node_a[formal], node_a[actual]);

#line 4204 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4204 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4205 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[formal]==TY_ARRAY)&&(node_kind[actual]==TY_ARRAY)))
#line 4205 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4205 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_match_generic(node_a[formal], node_a[actual]);

#line 4205 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4205 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[formal]==TY_DYN_ARRAY)&&(node_kind[actual]==TY_DYN_ARRAY)))
#line 4206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_match_generic(node_a[formal], node_a[actual]);

#line 4206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4207 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(formal);

#line 4208 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_type_equal(formal, actual)==0))
#line 4208 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);
else
#line 4208 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 4234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_substitute_type(int ty)
#line 4234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 4212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4217 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_PARAM))
#line 4217 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int b = tc_bind_find(node_value[ty]);

#line 4215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((b!=0))
#line 4215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_substitute_type(b);
else
#line 4215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4216 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_PARAM, node_a[ty], node_b[ty], node_c[ty], node_value[ty], node_aux[ty]);
}
else
#line 4217 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_PTR))
#line 4218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_PTR, tc_substitute_type(node_a[ty]), 0, 0, 0, 0);
else
#line 4218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_ARRAY))
#line 4219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_ARRAY, tc_substitute_type(node_a[ty]), 0, 0, node_value[ty], 0);
else
#line 4219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_DYN_ARRAY))
#line 4220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_DYN_ARRAY, tc_substitute_type(node_a[ty]), 0, 0, 0, 0);
else
#line 4220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4227 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_GENERIC))
#line 4227 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4222 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int args = 0;

#line 4222 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_a[ty];

#line 4223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 4223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int q = tc_substitute_type(p);

#line 4223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((args==0))
#line 4223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = q;
else
#line 4223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = ast_link(args, q);

#line 4223 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 4224 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int result = ast_node(TY_GENERIC, args, 0, 0, node_value[ty], 0);

#line 4225 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_scope[result] = node_scope[ty];

#line 4226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return result;
}
else
#line 4227 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4232 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_TUPLE))
#line 4232 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4229 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int items = 0;

#line 4229 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_a[ty];

#line 4230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 4230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int q = tc_substitute_type(p);

#line 4230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((items==0))
#line 4230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = q;
else
#line 4230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
items = ast_link(items, q);

#line 4230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 4231 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_TUPLE, items, 0, 0, 0, 0);
}
else
#line 4232 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4233 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(node_kind[ty], node_a[ty], node_b[ty], node_c[ty], node_value[ty], node_aux[ty]);
}

#line 4249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_same(int a_kind, int a_name, int b_kind, int b_name)
#line 4249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4237 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_kind==TY_PTR)&&(b_kind==TY_PTR)))
#line 4237 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4237 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4244 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_kind==b_kind))
#line 4244 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_kind==TY_NAMED))
#line 4242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_name==b_name))
#line 4240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4241 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 4242 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4244 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4245 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_is_integer_kind(a_kind)==1)&&(tc_is_integer_kind(b_kind)==1)))
#line 4245 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4245 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4246 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_kind==TY_VOID)&&(b_kind==TY_INT)))
#line 4246 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4246 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_kind==TY_INT)&&(b_kind==TY_VOID)))
#line 4247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4248 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4255 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_array_elem_same(int a_kind, int a_name, int b_kind, int b_name)
#line 4255 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4251 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_is_legacy_integer_kind(a_kind)==1)&&(tc_is_legacy_integer_kind(b_kind)==1)))
#line 4251 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4251 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_kind!=b_kind))
#line 4252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_kind==TY_NAMED))
#line 4253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_name==b_name))
#line 4253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 4253 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4254 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 4287 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_same_full(int a_kind, int a_name, int a_elem_kind, int a_elem_name, int b_kind, int b_name, int b_elem_kind, int b_elem_name)
#line 4287 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4260 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_kind==TY_PARAM)||(b_kind==TY_PARAM)))
#line 4260 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4258 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_kind==TY_PARAM)&&(b_kind==TY_PARAM)))
#line 4258 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4258 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4259 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4260 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_kind==TY_GENERIC)||(b_kind==TY_GENERIC)))
#line 4264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4262 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_kind==TY_GENERIC)&&(b_kind==TY_GENERIC)))
#line 4262 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_type_equal(a_name, b_name);
else
#line 4262 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 4264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4268 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_kind==TY_TUPLE)||(b_kind==TY_TUPLE)))
#line 4268 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4266 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_kind==TY_TUPLE)&&(b_kind==TY_TUPLE)))
#line 4266 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_type_equal(a_name, b_name);
else
#line 4266 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4267 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 4268 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4274 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_kind==TY_PTR)&&(b_kind==TY_PTR)))
#line 4274 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4270 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_elem_kind==TY_VOID)||(b_elem_kind==TY_VOID)))
#line 4270 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4270 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4271 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_elem_kind!=b_elem_kind))
#line 4271 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4271 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4272 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_elem_kind==TY_NAMED)&&(a_elem_name!=b_elem_name)))
#line 4272 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4272 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4273 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4274 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4282 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_kind==b_kind))
#line 4282 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_kind==TY_NAMED))
#line 4276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_name==b_name))
#line 4276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 4276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4280 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_kind==TY_DYN_ARRAY))
#line 4280 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4278 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_elem_kind!=b_elem_kind))
#line 4278 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4278 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_elem_kind==TY_NAMED)&&(a_elem_name!=b_elem_name)))
#line 4279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4280 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4281 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4282 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4283 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_is_integer_kind(a_kind)==1)&&(tc_is_integer_kind(b_kind)==1)))
#line 4283 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4283 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4284 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((a_kind==TY_CHAR)&&((((b_kind==TY_INT)||(b_kind==TY_BOOL))||(b_kind==TY_LONG))||(b_kind==TY_LLONG)))||((b_kind==TY_CHAR)&&((((a_kind==TY_INT)||(a_kind==TY_BOOL))||(a_kind==TY_LONG))||(a_kind==TY_LLONG)))))
#line 4284 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4284 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4285 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((a_kind==TY_VOID)&&(b_kind==TY_INT))||((a_kind==TY_INT)&&(b_kind==TY_VOID))))
#line 4285 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4285 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4286 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4295 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_ptr_diff_ok(int a_kind, int a_elem_kind, int a_elem_name, int b_kind, int b_elem_kind, int b_elem_name)
#line 4295 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4290 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_kind!=TY_PTR)||(b_kind!=TY_PTR)))
#line 4290 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4290 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4291 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_elem_kind==TY_VOID)||(b_elem_kind==TY_VOID)))
#line 4291 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4291 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((a_elem_kind!=b_elem_kind))
#line 4292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4293 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a_elem_kind==TY_NAMED)&&(a_elem_name!=b_elem_name)))
#line 4293 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4293 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4294 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 4307 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sym_suffix_equal(int full, int base)
#line 4307 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4301 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((sym_len[full]==sym_len[base]))
#line 4301 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 4299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<sym_len[base]))
#line 4299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(sym_start[full]+i)]!=source[(sym_start[base]+i)]))
#line 4299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 4300 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4301 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4302 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((sym_len[full]<(sym_len[base]+3)))
#line 4302 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4302 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int start = ((sym_start[full]+sym_len[full])-sym_len[base]);

#line 4304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((source[(start-1)]!=58)||(source[(start-2)]!=58)))
#line 4304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int j = 0;

#line 4305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((j<sym_len[base]))
#line 4305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[(start+j)]!=source[(sym_start[base]+j)]))
#line 4305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
j = (j+1);
}

#line 4306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 4316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_struct(int name)
#line 4316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4310 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[tc_root];

#line 4314 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 4314 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[item]==N_STRUCT)||(node_kind[item]==N_GENERIC_STRUCT))&&(node_value[item]==name)))
#line 4312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return item;
else
#line 4312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4313 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 4315 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4335 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_struct_ctx(int name, int ns)
#line 4335 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int exact = tc_find_struct(name);

#line 4320 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((exact!=0))
#line 4320 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return exact;
else
#line 4320 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4328 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ns==0))
#line 4328 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int broad = node_a[tc_root];

#line 4326 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((broad!=0))
#line 4326 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4324 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[broad]==N_STRUCT)||(node_kind[broad]==N_GENERIC_STRUCT))&&(sym_suffix_equal(node_value[broad], name)==1)))
#line 4324 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return broad;
else
#line 4324 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4325 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
broad = node_next[broad];
}

#line 4327 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 4328 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4329 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[tc_root];

#line 4333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 4333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((node_kind[item]==N_STRUCT)||(node_kind[item]==N_GENERIC_STRUCT))&&(node_scope[item]==ns))&&(sym_suffix_equal(node_value[item], name)==1)))
#line 4331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return item;
else
#line 4331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4332 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 4334 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4344 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_enum(int name)
#line 4344 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4338 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[tc_root];

#line 4342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 4342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4340 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[item]==N_ENUM)&&(node_value[item]==name)))
#line 4340 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return item;
else
#line 4340 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4341 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 4343 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4363 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_enum_ctx(int name, int ns)
#line 4363 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4347 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int exact = tc_find_enum(name);

#line 4348 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((exact!=0))
#line 4348 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return exact;
else
#line 4348 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4356 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ns==0))
#line 4356 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4350 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int broad = node_a[tc_root];

#line 4354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((broad!=0))
#line 4354 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[broad]==N_ENUM)&&(sym_suffix_equal(node_value[broad], name)==1)))
#line 4352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return broad;
else
#line 4352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4353 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
broad = node_next[broad];
}

#line 4355 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 4356 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4357 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[tc_root];

#line 4361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 4361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[item]==N_ENUM)&&(node_scope[item]==ns))&&(sym_suffix_equal(node_value[item], name)==1)))
#line 4359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return item;
else
#line 4359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 4362 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_generic_arity(int decl)
#line 4370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4366 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((decl==0)||(node_kind[decl]!=N_GENERIC_STRUCT)))
#line 4366 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4366 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = 0;

#line 4367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_c[decl];

#line 4368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 4368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
n = (n+1);

#line 4368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 4369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return n;
}

#line 4377 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_generic_arg_count(int ty)
#line 4377 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4373 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ty==0)||(node_kind[ty]!=TY_GENERIC)))
#line 4373 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4373 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4374 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int n = 0;

#line 4374 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_a[ty];

#line 4375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 4375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
n = (n+1);

#line 4375 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 4376 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return n;
}

#line 4383 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_named_exists_ctx(int name, int ns)
#line 4383 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4380 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_find_struct_ctx(name, ns)!=0))
#line 4380 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4380 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_find_enum_ctx(name, ns)!=0))
#line 4381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4382 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4389 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_named_exists(int name)
#line 4389 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4386 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_find_struct(name)!=0))
#line 4386 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4386 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4387 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_find_enum(name)!=0))
#line 4387 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4387 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4388 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4415 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_check_type(int ty)
#line 4415 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 4392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(1);

#line 4392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4393 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int k = node_kind[ty];

#line 4414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==TY_NAMED))
#line 4396 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_named_exists_ctx(node_value[ty], node_scope[ty])==0))
#line 4395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(2);
else
#line 4395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==TY_GENERIC))
#line 4403 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int s = tc_find_struct_ctx(node_value[ty], node_scope[ty]);

#line 4402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((s==0)||(node_kind[s]!=N_GENERIC_STRUCT)))
#line 4398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(2);
else
#line 4402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4400 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_generic_arity(s)!=tc_generic_arg_count(ty)))
#line 4400 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(37);
else
#line 4400 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = node_a[ty];

#line 4401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((a!=0))
#line 4401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_check_type(a);

#line 4401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}
}
}
else
#line 4414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==TY_PTR))
#line 4403 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_check_type(node_a[ty]);
else
#line 4414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==TY_ARRAY))
#line 4404 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_check_type(node_a[ty]);
else
#line 4414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==TY_DYN_ARRAY))
#line 4405 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_check_type(node_a[ty]);
else
#line 4414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==TY_TUPLE))
#line 4409 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4407 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[ty];

#line 4408 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 4408 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4408 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_check_type(item);

#line 4408 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}
}
else
#line 4414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==TY_FUN))
#line 4414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4411 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_a[ty];

#line 4412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 4412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_check_type(p);

#line 4412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 4413 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_check_type(node_b[ty]);
}
else
#line 4414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 4436 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_cycle_struct(int name)
#line 4436 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4418 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 4422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<tc_path_count))
#line 4422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4420 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_path_name[i]==name))
#line 4420 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4420 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4421 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 4423 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int s = tc_find_struct(name);

#line 4424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((s==0))
#line 4424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4425 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_tc_path(tc_path_count);

#line 4426 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_path_name[tc_path_count] = name;

#line 4427 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_path_count = (tc_path_count+1);

#line 4428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[s];

#line 4429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bad = 0;

#line 4433 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((f!=0))
#line 4433 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4431 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_cycle_type(node_b[f])==1))
#line 4431 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
bad = 1;
else
#line 4431 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4432 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];
}

#line 4434 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_path_count = (tc_path_count-1);

#line 4435 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return bad;
}

#line 4446 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_cycle_type(int ty)
#line 4446 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4438 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 4438 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4438 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4439 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_PTR))
#line 4439 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4439 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4440 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_FUN))
#line 4440 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4440 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4441 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_ARRAY))
#line 4441 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_cycle_type(node_a[ty]);
else
#line 4441 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4442 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_DYN_ARRAY))
#line 4442 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4442 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4443 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_NAMED))
#line 4443 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_cycle_struct(node_value[ty]);
else
#line 4443 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4444 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ty]==TY_GENERIC))
#line 4444 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4444 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4445 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_release_name(int name)
#line 4452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4450 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bi_has_flag(name, BI_FLAG_CONSUME)==1))
#line 4450 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4450 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4451 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4458 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_owned_initializer(int id)
#line 4458 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4455 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((id==0)||(node_kind[id]!=N_CALL)))
#line 4455 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4455 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4456 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bi_has_flag(node_value[id], BI_FLAG_OWNED)==1))
#line 4456 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4456 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4457 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4462 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_is_owner_kind(int kind)
#line 4462 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4460 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==TY_DYN_ARRAY))
#line 4460 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4460 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4461 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_borrow_conflict(int index)
#line 4467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((index<0))
#line 4464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4465 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((index<tc_var_count)&&(tc_var_borrow_count[index]>0)))
#line 4465 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 4465 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4466 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4476 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_move_var(int index)
#line 4476 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((index<0))
#line 4469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(5);

#line 4469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((index<tc_var_count))
#line 4470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 4470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(5);

#line 4470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}

#line 4471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_borrow_conflict(index)==1))
#line 4471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(37);

#line 4471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_var_moved[index]==1))
#line 4472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(34);

#line 4472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4472 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_var_owned[index]==0))
#line 4473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(35);

#line 4473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4474 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_moved[index] = 1;

#line 4475 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_owned[index] = 0;
}

#line 4481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_move_value(int id)
#line 4481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4478 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((id==0)||(node_kind[id]!=N_VAR)))
#line 4478 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 4478 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4479 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_lookup_var(node_value[id])==0))
#line 4479 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4479 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(5);

#line 4479 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4479 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_is_owner_kind(tc_kind)==1))
#line 4480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_move_var(tc_last_var_index);
else
#line 4480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 4489 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_record_borrow(int destination, int source_index2)
#line 4489 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((destination<0)||(source_index2<0)))
#line 4483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 4483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((destination<tc_var_count))
#line 4484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 4484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;

#line 4485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source_index2<tc_var_count))
#line 4485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 4485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;

#line 4486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_var_moved[source_index2]==1))
#line 4486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(33);

#line 4486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4487 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_borrow_source[destination] = source_index2;

#line 4488 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_borrow_count[source_index2] = (tc_var_borrow_count[source_index2]+1);
}

#line 4499 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_require_mutable(int id)
#line 4499 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((id==0))
#line 4491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 4491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_VAR))
#line 4494 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4493 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_lookup_var(node_value[id])==1)&&(tc_borrow_conflict(tc_last_var_index)==1)))
#line 4493 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(37);
else
#line 4493 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[id]==N_DEREF)||(node_kind[id]==N_INDEX))||(node_kind[id]==N_FIELD_ACCESS)))
#line 4498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(id);

#line 4496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_expr_borrow_source<0))
#line 4496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 4496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4497 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(37);
}
else
#line 4498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 4511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_consume_call(int id)
#line 4511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4502 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((id==0)||(node_kind[id]!=N_CALL))||(tc_release_name(node_value[id])==0)))
#line 4502 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 4502 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4503 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arg = node_a[id];

#line 4504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((arg==0)||(node_kind[arg]!=N_VAR)))
#line 4504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 4504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_lookup_var(node_value[arg])==0))
#line 4505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(5);

#line 4505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_last_var_moved==1))
#line 4506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(34);

#line 4506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_last_var_owned==0))
#line 4507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(35);

#line 4507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4507 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_borrow_conflict(tc_last_var_index)==1))
#line 4508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(37);

#line 4508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4509 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_moved[tc_last_var_index] = 1;

#line 4510 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_owned[tc_last_var_index] = 0;
}

#line 4537 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_add_var(int name, int kind, int named, int elem_kind, int elem_name, int type_node)
#line 4537 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4514 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int begin = 0;

#line 4515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_scope_count>0))
#line 4515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
begin = tc_scope_start[(tc_scope_count-1)];
else
#line 4515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4516 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = begin;

#line 4520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<tc_var_count))
#line 4520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_var_name[i]==name))
#line 4518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(3);

#line 4518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4519 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 4521 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_tc_vars(tc_var_count);

#line 4522 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_name[tc_var_count] = name;

#line 4523 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_kind[tc_var_count] = kind;

#line 4524 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_named[tc_var_count] = named;

#line 4525 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_elem_kind[tc_var_count] = elem_kind;

#line 4526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_elem_name[tc_var_count] = elem_name;

#line 4527 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_type[tc_var_count] = type_node;

#line 4528 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_owned[tc_var_count] = 0;

#line 4529 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_moved[tc_var_count] = 0;

#line 4530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_borrow_count[tc_var_count] = 0;

#line 4531 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_borrow_source[tc_var_count] = (0-1);

#line 4532 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_last_var_index = tc_var_count;

#line 4533 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_type[name] = kind;

#line 4534 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_elem_kind[name] = elem_kind;

#line 4535 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_elem_name[name] = elem_name;

#line 4536 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_count = (tc_var_count+1);
}

#line 4567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_lookup_var(int name)
#line 4567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_last_var_type = 0;

#line 4541 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_last_var_owned = 0;

#line 4542 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_last_var_moved = 0;

#line 4543 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr_borrow_source = (0-1);

#line 4544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_last_var_index = 0;

#line 4545 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = (tc_var_count-1);

#line 4565 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((1==1))
#line 4565 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4547 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((i<0))
#line 4547 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4547 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4563 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_var_name[i]==name))
#line 4563 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = tc_var_kind[i];

#line 4550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = tc_var_named[i];

#line 4551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = tc_var_elem_kind[i];

#line 4552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = tc_var_elem_name[i];

#line 4553 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_last_var_type = tc_var_type[i];

#line 4557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind==TY_DYN_ARRAY)&&(tc_last_var_type!=0)))
#line 4557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4555 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = node_kind[node_a[tc_last_var_type]];

#line 4556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_elem_kind==TY_NAMED))
#line 4556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = node_value[node_a[tc_last_var_type]];
else
#line 4556 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4558 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_last_var_owned = tc_var_owned[i];

#line 4559 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_last_var_moved = tc_var_moved[i];

#line 4560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr_borrow_source = tc_var_borrow_source[i];

#line 4561 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_last_var_index = i;

#line 4562 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4563 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4564 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i-1);
}

#line 4566 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 4587 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_type_node(int ty)
#line 4587 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4570 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_check_type(ty);

#line 4571 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ty;

#line 4572 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4572 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ty==0))
#line 4573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_VOID;

#line 4573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4574 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = node_kind[ty];

#line 4577 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind==TY_NAMED))
#line 4575 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4575 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = node_value[ty];
}
else
#line 4577 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((tc_kind==TY_GENERIC)||(tc_kind==TY_TUPLE))||(tc_kind==TY_PARAM)))
#line 4576 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4576 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = ty;
}
else
#line 4577 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4577 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;
}

#line 4586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind==TY_PTR)&&(node_a[ty]!=0)))
#line 4582 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4579 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = node_kind[node_a[ty]];

#line 4581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_elem_kind==TY_NAMED))
#line 4580 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4580 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = node_value[node_a[ty]];
}
else
#line 4581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_elem_kind==TY_GENERIC)||(tc_elem_kind==TY_PARAM)))
#line 4581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = node_a[ty];
}
else
#line 4581 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind==TY_DYN_ARRAY)&&(node_a[ty]!=0)))
#line 4586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4583 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = node_kind[node_a[ty]];

#line 4585 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_elem_kind==TY_NAMED))
#line 4584 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4584 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = node_value[node_a[ty]];
}
else
#line 4585 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_elem_kind==TY_GENERIC)||(tc_elem_kind==TY_PARAM)))
#line 4585 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4585 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = node_a[ty];
}
else
#line 4585 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 4605 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_numeric_result_kind(int a, int b)
#line 4605 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4590 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_DOUBLE)||(b==TY_DOUBLE)))
#line 4590 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_DOUBLE;
else
#line 4590 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4591 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_FLOAT)||(b==TY_FLOAT)))
#line 4591 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_FLOAT;
else
#line 4591 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4592 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==b)&&(tc_is_fixed_integer_kind(a)==1)))
#line 4592 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return a;
else
#line 4592 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4593 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_USIZE)||(b==TY_USIZE)))
#line 4593 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_USIZE;
else
#line 4593 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4594 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_U64)||(b==TY_U64)))
#line 4594 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_U64;
else
#line 4594 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4595 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_I64)||(b==TY_I64)))
#line 4595 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_I64;
else
#line 4595 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4596 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_U32)||(b==TY_U32)))
#line 4596 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_U32;
else
#line 4596 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_I32)||(b==TY_I32)))
#line 4597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_I32;
else
#line 4597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_U16)||(b==TY_U16)))
#line 4598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_U16;
else
#line 4598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4599 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_I16)||(b==TY_I16)))
#line 4599 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_I16;
else
#line 4599 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4600 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_U8)||(b==TY_U8)))
#line 4600 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_U8;
else
#line 4600 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4601 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_I8)||(b==TY_I8)))
#line 4601 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_I8;
else
#line 4601 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_LLONG)||(b==TY_LLONG)))
#line 4602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_LLONG;
else
#line 4602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_LONG)||(b==TY_LONG)))
#line 4603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_LONG;
else
#line 4603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4604 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
}

#line 4621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_integer_result_kind(int a, int b)
#line 4621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==b)&&(tc_is_fixed_integer_kind(a)==1)))
#line 4608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return a;
else
#line 4608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_USIZE)||(b==TY_USIZE)))
#line 4609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_USIZE;
else
#line 4609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4610 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_U64)||(b==TY_U64)))
#line 4610 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_U64;
else
#line 4610 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4611 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_I64)||(b==TY_I64)))
#line 4611 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_I64;
else
#line 4611 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_U32)||(b==TY_U32)))
#line 4612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_U32;
else
#line 4612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_I32)||(b==TY_I32)))
#line 4613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_I32;
else
#line 4613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4614 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_U16)||(b==TY_U16)))
#line 4614 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_U16;
else
#line 4614 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_I16)||(b==TY_I16)))
#line 4615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_I16;
else
#line 4615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_U8)||(b==TY_U8)))
#line 4616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_U8;
else
#line 4616 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4617 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_I8)||(b==TY_I8)))
#line 4617 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_I8;
else
#line 4617 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4618 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_LLONG)||(b==TY_LLONG)))
#line 4618 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_LLONG;
else
#line 4618 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((a==TY_LONG)||(b==TY_LONG)))
#line 4619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_LONG;
else
#line 4619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4620 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
}

#line 4643 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_check_variant(int id)
#line 4643 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_find_enum_variant(node_value[id])==0))
#line 4624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 4624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4625 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arg = node_a[id];

#line 4626 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = node_b[tc_variant_member];

#line 4634 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((arg!=0)&&(field!=0)))
#line 4634 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[field]);

#line 4628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int fk = tc_kind;

#line 4628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f_name = tc_name;

#line 4628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int fek = tc_elem_kind;

#line 4628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f_elem_name = tc_elem_name;

#line 4629 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(arg, fk);

#line 4630 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(arg);

#line 4630 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ak = tc_kind;

#line 4630 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int an = tc_name;

#line 4630 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aek = tc_elem_kind;

#line 4630 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aen = tc_elem_name;

#line 4631 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_literal_fits(arg, fk)==0))
#line 4631 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(54);
else
#line 4631 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_same_full(ak, an, aek, aen, fk, f_name, fek, f_elem_name)==0))
#line 4632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);

#line 4632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4633 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arg = node_next[arg];

#line 4633 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
field = node_next[field];
}

#line 4635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((arg!=0)||(field!=0)))
#line 4635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 4635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4636 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_NAMED;

#line 4637 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = tc_variant_enum;

#line 4638 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4640 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_NAMED, 0, 0, 0, tc_variant_enum, 0);

#line 4641 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux[id] = tc_result_type;

#line 4642 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 5099 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_expr(int id)
#line 5099 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = 0;

#line 4646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr_borrow_source = (0-1);

#line 4647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((id!=0))
#line 4647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_error_pos = node_pos[id];
else
#line 4647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((id==0))
#line 4648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(4);

#line 4648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4649 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int k = node_kind[id];

#line 4650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_INT))
#line 4650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_INT, 0, 0, 0, 0, 0);

#line 4650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_FLOAT))
#line 4651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_aux[id]==TY_FLOAT))
#line 4651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_FLOAT;

#line 4651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_FLOAT, 0, 0, 0, 0, 0);
}
else
#line 4651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_DOUBLE;

#line 4651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_DOUBLE, 0, 0, 0, 0, 0);
}

#line 4651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_CHAR))
#line 4652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_CHAR;

#line 4652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_CHAR, 0, 0, 0, 0, 0);

#line 4652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_NULL))
#line 4653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 4653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = TY_VOID;

#line 4653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_PTR, ast_node(TY_VOID, 0, 0, 0, 0, 0), 0, 0, 0, 0);

#line 4653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_BOOL))
#line 4654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_BOOL;

#line 4654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_BOOL, 0, 0, 0, 0, 0);

#line 4654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_STRING))
#line 4655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_STRING;

#line 4655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_STRING, 0, 0, 0, 0, 0);

#line 4655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4656 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_VARIANT))
#line 4656 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4656 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_check_variant(id);

#line 4656 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4656 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_TUPLE))
#line 4676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4658 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[id];

#line 4659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int types = 0;

#line 4667 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 4667 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4661 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(item);

#line 4662 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int elem_ty = tc_result_type;

#line 4663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((elem_ty==0))
#line 4663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
elem_ty = tc_type_node_from_summary(tc_kind, tc_name, tc_elem_kind, tc_elem_name);
else
#line 4663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((elem_ty==0))
#line 4664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(19);

#line 4664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((types==0))
#line 4665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
types = elem_ty;
else
#line 4665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
types = ast_link(types, elem_ty);

#line 4666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 4668 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tuple_ty = ast_node(TY_TUPLE, types, 0, 0, 0, 0);

#line 4669 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_TUPLE;

#line 4670 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = tuple_ty;

#line 4671 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4672 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4673 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = tuple_ty;

#line 4674 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux[id] = tuple_ty;

#line 4675 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_VAR))
#line 4682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_lookup_var(node_value[id])==1))
#line 4678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_last_var_moved==1))
#line 4678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(33);
else
#line 4678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr_borrow_source = tc_var_borrow_source[tc_last_var_index];

#line 4678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = tc_last_var_type;

#line 4678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux[id] = tc_last_var_type;

#line 4678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int e = tc_find_enum_value(node_value[id]);

#line 4680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((e!=0))
#line 4680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_NAMED;

#line 4680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = e;

#line 4680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_NAMED, 0, 0, 0, e, 0);

#line 4680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux[id] = tc_result_type;

#line 4680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4681 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_error_symbol = node_value[id];

#line 4681 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(5);

#line 4681 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_ADDRESS))
#line 4688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int address_entry = 0;

#line 4685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[node_a[id]]==N_VAR))
#line 4685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
address_entry = tc_find_function_ctx(node_value[node_a[id]], node_scope[node_a[id]]);
else
#line 4685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((address_entry!=0))
#line 4686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_FUN;

#line 4686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = tc_signature_type(address_entry);

#line 4686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int oldk = tc_kind;

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int oldn = tc_name;

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int olde = tc_elem_kind;

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int olden = tc_elem_name;

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[node_a[id]]==N_VAR)&&(tc_lookup_var(node_value[node_a[id]])==1)))
#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr_borrow_source = tc_last_var_index;
else
#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = oldn;

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = oldk;

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = oldn;

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((oldk==TY_PTR))
#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = olde;

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = olden;
}
else
#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_DEREF))
#line 4695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4690 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 4691 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int deref_borrow = tc_expr_borrow_source;

#line 4693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_PTR))
#line 4692 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(6);
else
#line 4693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = tc_elem_kind;

#line 4693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = tc_elem_name;

#line 4693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr_borrow_source = deref_borrow;
}

#line 4694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4715 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_INDEX))
#line 4715 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4697 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_b[id]);

#line 4698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(7);
else
#line 4698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 4700 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int index_borrow = tc_expr_borrow_source;

#line 4713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind==TY_ARRAY))
#line 4709 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ix = node_b[id];

#line 4703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ik = (0-1);

#line 4704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int is_const = 0;

#line 4706 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[ix]==N_INT))
#line 4705 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4705 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ik = node_value[ix];

#line 4705 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
is_const = 1;
}
else
#line 4706 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((node_kind[ix]==N_BINOP)&&(node_value[ix]==OP_SUB))&&(node_kind[node_a[ix]]==N_INT))&&(node_value[node_a[ix]]==0))&&(node_kind[node_b[ix]]==N_INT)))
#line 4706 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4706 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ik = (0-node_value[node_b[ix]]);

#line 4706 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
is_const = 1;
}
else
#line 4706 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4707 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((is_const==1)&&(tc_result_type!=0))&&(node_kind[tc_result_type]==TY_ARRAY))&&((ik<0)||(ik>(node_value[tc_result_type]-1)))))
#line 4707 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(45);
else
#line 4707 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4708 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4708 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4708 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4708 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4708 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr_borrow_source = (0-1);
}
else
#line 4713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind==TY_DYN_ARRAY))
#line 4710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ek = tc_elem_kind;

#line 4710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int en = tc_elem_name;

#line 4710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = ek;

#line 4710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = en;

#line 4710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr_borrow_source = index_borrow;
}
else
#line 4713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind==TY_PTR))
#line 4711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = tc_elem_kind;

#line 4711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = tc_elem_name;

#line 4711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr_borrow_source = index_borrow;
}
else
#line 4713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind==TY_STRING))
#line 4712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_CHAR;

#line 4712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr_borrow_source = (0-1);
}
else
#line 4713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4714 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4715 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4779 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_FIELD_ACCESS))
#line 4779 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4717 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 4718 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int base_kind = tc_kind;

#line 4718 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int base_name = tc_name;

#line 4722 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((base_kind==TY_DYN_ARRAY))
#line 4722 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bi_has_flag(node_value[id], BI_FLAG_DYNFIELD)==1))
#line 4720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4721 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(11);

#line 4721 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4722 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((base_kind==TY_PTR))
#line 4726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4725 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_elem_kind==TY_GENERIC))
#line 4724 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4724 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base_kind = TY_GENERIC;

#line 4724 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base_name = tc_elem_name;
}
else
#line 4725 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4725 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base_kind = TY_NAMED;

#line 4725 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base_name = tc_elem_name;
}
}
else
#line 4726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4735 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((base_kind==TY_VARIANT))
#line 4735 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int variant = base_name;

#line 4729 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int payload_field = node_b[variant];

#line 4733 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((payload_field!=0))
#line 4733 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4731 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[payload_field]==node_value[id]))
#line 4731 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4731 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[payload_field]);

#line 4731 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4731 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
payload_field = node_next[payload_field];
}

#line 4734 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(11);

#line 4734 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4735 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4755 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((base_kind==TY_NAMED))
#line 4755 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_value[id]==sym_tag_id())&&(tc_find_enum(base_name)!=0)))
#line 4737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int enum_decl = tc_find_enum(base_name);

#line 4754 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((enum_decl!=0))
#line 4754 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4740 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int variant_item = node_a[enum_decl];

#line 4753 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((variant_item!=0))
#line 4753 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4751 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[variant_item]==node_value[id]))
#line 4751 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_b[variant_item]==0))
#line 4743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(11);

#line 4743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4744 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_VARIANT;

#line 4745 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = variant_item;

#line 4746 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4747 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4748 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_VARIANT, 0, 0, 0, variant_item, base_name);

#line 4749 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux[id] = tc_result_type;

#line 4750 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4751 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4752 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
variant_item = node_next[variant_item];
}
}
else
#line 4754 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4755 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((base_kind==TY_GENERIC))
#line 4769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4757 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int base_ty = base_name;

#line 4758 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int sgen = tc_find_struct(node_value[base_ty]);

#line 4759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((sgen==0))
#line 4759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(10);

#line 4759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_bind_clear();

#line 4761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gp = node_c[sgen];

#line 4761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ga = node_a[base_ty];

#line 4762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((gp!=0)&&(ga!=0)))
#line 4762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_bind_add(node_a[gp], ga)==0))
#line 4762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 4762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gp = node_next[gp];

#line 4762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ga = node_next[ga];
}

#line 4763 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gf = node_a[sgen];

#line 4767 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((gf!=0))
#line 4767 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[gf]==node_value[id]))
#line 4765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int subst = tc_substitute_type(node_b[gf]);

#line 4765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(subst);

#line 4765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4766 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gf = node_next[gf];
}

#line 4768 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(11);

#line 4768 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((base_kind!=TY_NAMED))
#line 4770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(9);

#line 4770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4771 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int s = tc_find_struct(base_name);

#line 4772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((s==0))
#line 4772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(10);

#line 4772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4773 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[s];

#line 4777 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((f!=0))
#line 4777 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[f]==node_value[id]))
#line 4775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[f]);

#line 4775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4776 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];
}

#line 4778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(11);

#line 4778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4779 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5042 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_CALL))
#line 5042 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4781 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_check_variant(id)==1))
#line 4781 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4781 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_kind[id] = N_VARIANT;

#line 4781 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4781 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4782 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int call_name = node_value[id];

#line 4783 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int btag = bi_tag(call_name);

#line 4798 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_MEM_ALLOC))
#line 4798 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((aa==0)||(node_next[aa]==0))||(node_next[node_next[aa]]!=0)))
#line 4786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4789 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int witness = node_next[aa];

#line 4790 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(witness);

#line 4791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind==TY_VOID))
#line 4791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4792 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int wk = tc_kind;

#line 4792 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int wn = tc_name;

#line 4793 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int witness_ty = tc_result_type;

#line 4794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((witness_ty==0))
#line 4794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
witness_ty = tc_type_node_from_summary(wk, wn, tc_elem_kind, tc_elem_name);
else
#line 4794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4795 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 4795 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4795 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = wk;

#line 4795 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = wn;

#line 4796 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_PTR, witness_ty, 0, 0, 0, 0);

#line 4797 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4798 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4817 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_MEM_ALLOC_ALIGNED))
#line 4817 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4800 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((aa==0)||(node_next[aa]==0))||(node_next[node_next[aa]]==0))||(node_next[node_next[node_next[aa]]]!=0)))
#line 4801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4802 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4804 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];

#line 4805 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[aa]==N_INT)&&(node_value[aa]<1)))
#line 4807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];

#line 4809 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind==TY_VOID))
#line 4810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4810 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int wk = tc_kind;

#line 4811 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int wn = tc_name;

#line 4812 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int witness_ty = tc_result_type;

#line 4813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((witness_ty==0))
#line 4813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
witness_ty = tc_type_node_from_summary(wk, wn, tc_elem_kind, tc_elem_name);
else
#line 4813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 4814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = wk;

#line 4814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = wn;

#line 4815 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_PTR, witness_ty, 0, 0, 0, 0);

#line 4816 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4817 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4830 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_MEM_RESIZE))
#line 4830 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((aa==0)||(node_next[aa]==0))||(node_next[node_next[aa]]==0))||(node_next[node_next[node_next[aa]]]==0))||(node_next[node_next[node_next[node_next[aa]]]]!=0)))
#line 4820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4821 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_PTR))
#line 4822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pk = tc_elem_kind;

#line 4823 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pn = tc_elem_name;

#line 4824 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ptr_ty = tc_result_type;

#line 4825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];

#line 4825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4825 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];

#line 4826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4826 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4827 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];

#line 4827 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind==TY_VOID)||(tc_array_elem_same(pk, pn, tc_kind, tc_name)==0)))
#line 4828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(36);

#line 4828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4828 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 4829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = pk;

#line 4829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = pn;

#line 4829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ptr_ty;

#line 4829 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4830 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_MEM_FREE))
#line 4837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4832 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((aa==0)||(node_next[aa]!=0)))
#line 4833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4833 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4834 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_PTR))
#line 4835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4835 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_VOID;

#line 4836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = 0;

#line 4836 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4837 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4845 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_READ_LINE))
#line 4845 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4839 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((aa==0)||(node_next[aa]!=0)))
#line 4840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4840 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4841 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4842 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4842 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4842 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4842 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4842 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4843 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_STRING;

#line 4843 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4843 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4843 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4844 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4845 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_READ_INT))
#line 4853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4847 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4848 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((aa==0)||(node_next[aa]!=0)))
#line 4848 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4848 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4848 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4848 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4849 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4850 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4850 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4850 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4850 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4850 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4851 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4852 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4853 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((btag==BI_TC_WRITE_STRING)||(btag==BI_TC_WRITE_LINE)))
#line 4861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4855 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4856 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((aa==0)||(node_next[aa]!=0)))
#line 4856 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4856 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4856 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4856 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4857 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_STRING))
#line 4858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4858 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_VOID;

#line 4859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4859 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4860 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4861 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4869 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_WRITE_INT))
#line 4869 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4863 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4864 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((aa==0)||(node_next[aa]!=0)))
#line 4864 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4864 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4864 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4864 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4865 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4866 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4866 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4866 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4866 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4866 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4867 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_VOID;

#line 4867 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4867 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4867 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4868 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4869 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4877 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_WRITE_CHAR))
#line 4877 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4871 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4872 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((aa==0)||(node_next[aa]!=0)))
#line 4872 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4872 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4872 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4872 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4873 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4874 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_CHAR))
#line 4874 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4874 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4874 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4874 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4875 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_VOID;

#line 4875 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4875 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4875 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4876 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4877 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4882 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_IO_STATUS))
#line 4882 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4879 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[id]!=0))
#line 4879 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4879 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4879 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4879 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4880 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4880 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4880 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4880 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4881 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4882 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4889 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_ATOMIC_MAKE))
#line 4889 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4884 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4885 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((aa==0)||(node_next[aa]!=0)))
#line 4885 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4885 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4885 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4885 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4886 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4886 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4886 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4886 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4886 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4886 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4887 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 4887 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4887 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = TY_VOID;

#line 4887 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4888 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_PTR, ast_node(TY_VOID, 0, 0, 0, 0, 0), 0, 0, 0, 0);

#line 4888 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4889 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((btag==BI_TC_ATOMIC_LOAD)||(btag==BI_TC_ATOMIC_FREE)))
#line 4897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4891 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4892 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((aa==0)||(node_next[aa]!=0)))
#line 4892 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4892 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4892 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4892 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind!=TY_PTR)||(tc_elem_kind!=TY_VOID)))
#line 4893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4893 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4895 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_ATOMIC_FREE))
#line 4894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_VOID;

#line 4894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4894 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = 0;
}
else
#line 4895 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4895 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4895 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4895 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4895 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4895 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_INT, 0, 0, 0, 0, 0);
}

#line 4896 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4897 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4904 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_ATOMIC_FETCH_ADD))
#line 4904 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4899 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((aa==0)||(node_next[aa]==0))||(node_next[node_next[aa]]!=0)))
#line 4900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4900 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind!=TY_PTR)||(tc_elem_kind!=TY_VOID)))
#line 4901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4901 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4902 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];

#line 4902 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4902 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4902 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4902 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4902 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4902 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_INT, 0, 0, 0, 0, 0);

#line 4903 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4904 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4911 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_ATOMIC_STORE))
#line 4911 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4906 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4907 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((aa==0)||(node_next[aa]==0))||(node_next[node_next[aa]]!=0)))
#line 4907 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4907 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4907 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4907 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4908 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4908 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind!=TY_PTR)||(tc_elem_kind!=TY_VOID)))
#line 4908 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4908 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4908 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4908 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];

#line 4909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4909 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4910 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_VOID;

#line 4910 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4910 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4910 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4910 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = 0;

#line 4910 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4911 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4919 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_ATOMIC_CAS))
#line 4919 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4913 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4914 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((aa==0)||(node_next[aa]==0))||(node_next[node_next[aa]]==0))||(node_next[node_next[node_next[aa]]]!=0)))
#line 4914 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4914 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4914 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4914 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4915 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4915 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind!=TY_PTR)||(tc_elem_kind!=TY_VOID)))
#line 4915 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4915 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4915 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4915 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];

#line 4916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4916 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];

#line 4917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4917 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4918 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4918 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4918 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4918 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4918 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_INT, 0, 0, 0, 0, 0);

#line 4918 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4919 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4926 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_CHANNEL_MAKE))
#line 4926 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4921 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4922 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((aa==0)||(node_next[aa]!=0)))
#line 4922 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4922 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4922 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4922 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4923 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4924 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 4924 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4924 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = TY_VOID;

#line 4924 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4925 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_PTR, ast_node(TY_VOID, 0, 0, 0, 0, 0), 0, 0, 0, 0);

#line 4925 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4926 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4935 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((btag==BI_TC_CHANNEL_SEND)||(btag==BI_TC_CHANNEL_RECV)))
#line 4935 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4928 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4929 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((aa==0)||(node_next[aa]==0))||(node_next[node_next[aa]]!=0)))
#line 4929 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4929 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4929 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4929 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4930 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4930 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind!=TY_PTR)||(tc_elem_kind!=TY_VOID)))
#line 4930 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4930 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4930 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4930 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4931 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
aa = node_next[aa];

#line 4931 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4933 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_CHANNEL_SEND))
#line 4932 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4932 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_INT))
#line 4932 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4932 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);

#line 4932 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4932 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4933 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind!=TY_PTR)||(tc_elem_kind!=TY_INT)))
#line 4933 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4933 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4933 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4933 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4934 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4934 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4934 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4934 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4934 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_INT, 0, 0, 0, 0, 0);

#line 4934 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4935 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4941 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((btag==BI_TC_CHANNEL_CLOSE)||(btag==BI_TC_CHANNEL_FREE)))
#line 4941 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4937 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((aa==0)||(node_next[aa]!=0)))
#line 4938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4938 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind!=TY_PTR)||(tc_elem_kind!=TY_VOID)))
#line 4939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4939 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4940 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_VOID;

#line 4940 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4940 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4940 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4940 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = 0;

#line 4940 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4941 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4965 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_THREAD_SPAWN))
#line 4965 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4943 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4944 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((aa==0)||(node_next[aa]==0))||(node_next[node_next[aa]]!=0)))
#line 4944 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4944 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4944 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4944 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4945 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int callback_ok = 0;

#line 4959 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[aa]==N_ADDRESS)&&(node_kind[node_a[aa]]==N_VAR)))
#line 4952 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4947 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int entry = tc_find_function_ctx(node_value[node_a[aa]], node_scope[node_a[aa]]);

#line 4951 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((entry!=0)&&(node_b[entry]!=0))&&(node_kind[node_b[entry]]==TY_INT)))
#line 4951 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4949 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ep = node_c[entry];

#line 4950 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((ep!=0)&&(node_next[ep]==0))&&(node_kind[node_b[ep]]==TY_PTR))&&(node_a[node_b[ep]]!=0))&&(node_kind[node_a[node_b[ep]]]==TY_VOID)))
#line 4950 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
callback_ok = 1;
else
#line 4950 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4951 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4959 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4953 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4954 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int callback_ty = tc_result_type;

#line 4958 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((tc_kind==TY_FUN)&&(callback_ty!=0))&&(node_kind[callback_ty]==TY_FUN))&&(node_b[callback_ty]!=0))&&(node_kind[node_b[callback_ty]]==TY_INT)))
#line 4958 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4956 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ep = node_a[callback_ty];

#line 4957 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((ep!=0)&&(node_next[ep]==0))&&(node_kind[ep]==TY_PTR))&&(node_a[ep]!=0))&&(node_kind[node_a[ep]]==TY_VOID)))
#line 4957 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
callback_ok = 1;
else
#line 4957 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 4958 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 4960 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((callback_ok==0))
#line 4960 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4960 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);

#line 4960 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4960 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4961 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arg = node_next[aa];

#line 4961 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(arg);

#line 4962 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind!=TY_PTR)||(tc_elem_kind!=TY_VOID)))
#line 4962 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4962 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4962 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4962 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4963 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 4963 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4963 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = TY_VOID;

#line 4963 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4964 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_PTR, ast_node(TY_VOID, 0, 0, 0, 0, 0), 0, 0, 0, 0);

#line 4964 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4965 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4971 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_THREAD_JOIN))
#line 4971 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4967 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aa = node_a[id];

#line 4968 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((aa==0)||(node_next[aa]!=0)))
#line 4968 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4968 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4968 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4968 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4969 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(aa);

#line 4969 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_kind!=TY_PTR)||(tc_elem_kind!=TY_VOID)))
#line 4969 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4969 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(8);

#line 4969 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4969 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = ast_node(TY_INT, 0, 0, 0, 0, 0);

#line 4970 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4971 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4975 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_THREAD_YIELD))
#line 4975 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4973 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[id]!=0))
#line 4973 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4973 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 4973 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4973 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4974 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_VOID;

#line 4974 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4974 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 4974 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4974 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = 0;

#line 4974 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4975 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4976 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int fun_node = tc_find_function_ctx(node_value[id], node_scope[id]);

#line 4999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((fun_node==0))
#line 4999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4992 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_lookup_var(node_value[id])==1)&&(tc_kind==TY_FUN)))
#line 4992 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4979 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int fty = tc_last_var_type;

#line 4980 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((fty==0)||(node_kind[fty]!=TY_FUN)))
#line 4980 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4980 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(42);

#line 4980 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4980 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4981 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arg_fp = node_a[id];

#line 4981 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p_fp = node_a[fty];

#line 4989 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((arg_fp!=0)&&(p_fp!=0)))
#line 4989 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4983 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(arg_fp);

#line 4983 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ak_fp = tc_kind;

#line 4983 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int an_fp = tc_name;

#line 4983 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aek_fp = tc_elem_kind;

#line 4983 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aen_fp = tc_elem_name;

#line 4984 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(p_fp);

#line 4984 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pek_fp = tc_kind;

#line 4984 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pen_fp = tc_name;

#line 4984 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int peek_fp = tc_elem_kind;

#line 4984 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int peen_fp = tc_elem_name;

#line 4985 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_literal_fits(arg_fp, pek_fp)==0))
#line 4985 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(54);
else
#line 4985 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4986 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((pek_fp==TY_DYN_ARRAY)&&(ak_fp==TY_DYN_ARRAY)))
#line 4986 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_move_value(arg_fp);
else
#line 4986 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4987 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_same_full(ak_fp, an_fp, aek_fp, aen_fp, pek_fp, pen_fp, peek_fp, peen_fp)==0))
#line 4987 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);
else
#line 4987 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4988 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arg_fp = node_next[arg_fp];

#line 4988 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p_fp = node_next[p_fp];
}

#line 4990 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((arg_fp!=0)||(p_fp!=0)))
#line 4990 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);
else
#line 4990 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4991 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[fty]);

#line 4991 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4992 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4993 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_PTR_INT))
#line 4993 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4993 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 4993 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4993 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = TY_INT;

#line 4993 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4993 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4993 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4994 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_VOID))
#line 4994 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4994 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_VOID;

#line 4994 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4994 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4994 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_PTR_VOID))
#line 4995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 4995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = TY_VOID;

#line 4995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;

#line 4995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4995 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4996 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_INT))
#line 4996 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4996 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 4996 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4996 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4996 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((btag==BI_TC_STRING))
#line 4997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 4997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_STRING;

#line 4997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 4997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4997 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 4998 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(41);

#line 4998 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 4999 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5024 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[fun_node]==N_GENERIC_FUNC))
#line 5024 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5001 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_bind_clear();

#line 5002 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int generic_moves_array = tc_generic_moves_array(fun_node);

#line 5003 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ga = node_a[id];

#line 5003 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gp = node_c[fun_node];

#line 5018 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((ga!=0)&&(gp!=0)))
#line 5018 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5005 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int formal_arg = node_b[gp];

#line 5010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[formal_arg]==TY_PARAM))
#line 5010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5007 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bound_arg = tc_bind_find(node_value[formal_arg]);

#line 5009 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((bound_arg!=0)&&(node_kind[bound_arg]==TY_FLOAT)))
#line 5008 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(ga, TY_FLOAT);
else
#line 5009 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((bound_arg!=0)&&(node_kind[bound_arg]==TY_DOUBLE)))
#line 5009 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(ga, TY_DOUBLE);
else
#line 5009 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5010 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5011 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(ga);

#line 5012 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int actual_kind = tc_kind;

#line 5013 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int actual_ty = tc_result_type;

#line 5014 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((generic_moves_array==1)&&(actual_kind==TY_DYN_ARRAY)))
#line 5014 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_move_value(ga);
else
#line 5014 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5015 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((actual_ty==0))
#line 5015 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
actual_ty = tc_type_node_from_summary(tc_kind, tc_name, tc_elem_kind, tc_elem_name);
else
#line 5015 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5016 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_match_generic_call_arg(formal_arg, actual_ty, ga);

#line 5017 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ga = node_next[ga];

#line 5017 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gp = node_next[gp];
}

#line 5019 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ga!=0)||(gp!=0)))
#line 5019 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);
else
#line 5019 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5020 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int generic_ret = tc_substitute_type(node_b[fun_node]);

#line 5021 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(generic_ret);

#line 5022 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux[id] = tc_result_type;

#line 5023 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 5024 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5025 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arg = node_a[id];

#line 5025 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_c[fun_node];

#line 5039 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((arg!=0)&&(p!=0)))
#line 5039 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5027 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[p]);

#line 5027 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pek = tc_kind;

#line 5027 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pen = tc_name;

#line 5027 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int peek = tc_elem_kind;

#line 5027 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int peen = tc_elem_name;

#line 5027 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int formal_type = tc_result_type;

#line 5028 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(arg, pek);

#line 5029 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(arg);

#line 5029 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ak = tc_kind;

#line 5029 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int an = tc_name;

#line 5029 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aek = tc_elem_kind;

#line 5029 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aen = tc_elem_name;

#line 5029 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int actual_type = tc_result_type;

#line 5030 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_literal_fits(arg, pek)==0))
#line 5030 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(54);
else
#line 5030 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5031 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((pek==TY_DYN_ARRAY)&&(ak==TY_DYN_ARRAY)))
#line 5031 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_move_value(arg);
else
#line 5031 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5032 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_same_full(ak, an, aek, aen, pek, pen, peek, peen)==0))
#line 5032 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);
else
#line 5032 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5037 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ak==TY_FUN)&&(pek==TY_FUN)))
#line 5037 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5034 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((actual_type==0))
#line 5034 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);
else
#line 5034 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5035 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((formal_type==0))
#line 5035 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);
else
#line 5035 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5036 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((actual_type!=0)&&(formal_type!=0))&&(tc_type_equal(actual_type, formal_type)==0)))
#line 5036 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);
else
#line 5036 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5037 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5038 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arg = node_next[arg];

#line 5038 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 5040 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((arg!=0)||(p!=0)))
#line 5040 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);
else
#line 5040 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[fun_node]);

#line 5041 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 5042 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5058 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_INDIRECT_CALL))
#line 5058 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5044 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 5045 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((tc_kind!=TY_FUN)||(tc_result_type==0))||(node_kind[tc_result_type]!=TY_FUN)))
#line 5045 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5045 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);

#line 5045 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 5045 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5046 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int fty = tc_result_type;

#line 5047 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arg = node_b[id];

#line 5047 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int param = node_a[fty];

#line 5055 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((arg!=0)&&(param!=0)))
#line 5055 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5049 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(param);

#line 5049 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pk = tc_kind;

#line 5049 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pn = tc_name;

#line 5049 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pek = tc_elem_kind;

#line 5049 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pen = tc_elem_name;

#line 5050 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(arg, pk);

#line 5051 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(arg);

#line 5051 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ak = tc_kind;

#line 5051 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int an = tc_name;

#line 5051 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aek = tc_elem_kind;

#line 5051 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aen = tc_elem_name;

#line 5052 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_literal_fits(arg, pk)==0))
#line 5052 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(54);
else
#line 5052 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5053 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_same_full(ak, an, aek, aen, pk, pn, pek, pen)==0))
#line 5053 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(12);
else
#line 5053 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5054 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arg = node_next[arg];

#line 5054 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
param = node_next[param];
}

#line 5056 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((arg!=0)||(param!=0)))
#line 5056 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5056 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(13);

#line 5056 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 5056 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5057 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[fty]);

#line 5057 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 5058 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_BINOP))
#line 5097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 5060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ak = tc_kind;

#line 5060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int an = tc_name;

#line 5060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ae = tc_elem_kind;

#line 5060 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int aen = tc_elem_name;

#line 5061 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_b[id]);

#line 5061 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bk = tc_kind;

#line 5061 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bn = tc_name;

#line 5061 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int be = tc_elem_kind;

#line 5061 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ben = tc_elem_name;

#line 5093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_value[id]==OP_CONCAT))
#line 5065 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ak!=TY_STRING)||(bk!=TY_STRING)))
#line 5063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(14);
else
#line 5063 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5064 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_STRING;

#line 5064 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;
}
else
#line 5093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_value[id]==OP_AND)||(node_value[id]==OP_OR)))
#line 5068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5066 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_is_integer_kind(ak)==0)||(tc_is_integer_kind(bk)==0)))
#line 5066 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(15);
else
#line 5066 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5067 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_BOOL;

#line 5067 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;
}
else
#line 5093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((node_value[id]==OP_BITAND)||(node_value[id]==OP_BITOR))||(node_value[id]==OP_BITXOR))||(node_value[id]==OP_SHL))||(node_value[id]==OP_SHR)))
#line 5068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_is_integer_kind(ak)==0)||(tc_is_integer_kind(bk)==0)))
#line 5068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(32);
else
#line 5068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = tc_integer_result_kind(ak, bk);

#line 5068 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;
}
else
#line 5093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_value[id]==OP_EQ)||(node_value[id]==OP_NEQ)))
#line 5074 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5069 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int null_cmp = 0;

#line 5070 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((ak==TY_PTR)&&(bk==TY_INT))&&(node_kind[node_b[id]]==N_INT))&&(node_value[node_b[id]]==0)))
#line 5070 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
null_cmp = 1;
else
#line 5070 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5071 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((ak==TY_INT)&&(bk==TY_PTR))&&(node_kind[node_a[id]]==N_INT))&&(node_value[node_a[id]]==0)))
#line 5071 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
null_cmp = 1;
else
#line 5071 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5072 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_same_full(ak, an, ae, aen, bk, bn, be, ben)==0)&&(null_cmp==0)))
#line 5072 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(16);
else
#line 5072 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5073 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_BOOL;

#line 5073 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;
}
else
#line 5093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_value[id]==OP_LT)||(node_value[id]==OP_GT)))
#line 5077 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5075 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_is_numeric_kind(ak)==0)||(tc_is_numeric_kind(bk)==0)))
#line 5075 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(17);
else
#line 5075 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5076 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_BOOL;

#line 5076 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;
}
else
#line 5093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_value[id]==OP_ADD))
#line 5082 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5081 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((ak==TY_PTR)&&(tc_is_integer_kind(bk)==1))&&(ae!=TY_VOID)))
#line 5078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 5078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = an;

#line 5078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = ae;

#line 5078 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = aen;
}
else
#line 5081 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((tc_is_integer_kind(ak)==1)&&(bk==TY_PTR))&&(be!=TY_VOID)))
#line 5079 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5079 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 5079 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = bn;

#line 5079 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = be;

#line 5079 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = ben;
}
else
#line 5081 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_is_numeric_kind(ak)==1)&&(tc_is_numeric_kind(bk)==1)))
#line 5080 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5080 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = tc_numeric_result_kind(ak, bk);

#line 5080 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;
}
else
#line 5081 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(18);
}
else
#line 5093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_value[id]==OP_SUB))
#line 5087 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((ak==TY_PTR)&&(tc_is_integer_kind(bk)==1))&&(ae!=TY_VOID)))
#line 5083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_PTR;

#line 5083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = an;

#line 5083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = ae;

#line 5083 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = aen;
}
else
#line 5086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_ptr_diff_ok(ak, ae, aen, bk, be, ben)==1))
#line 5084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = TY_INT;

#line 5084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;

#line 5084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_kind = 0;

#line 5084 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_elem_name = 0;
}
else
#line 5086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_is_numeric_kind(ak)==1)&&(tc_is_numeric_kind(bk)==1)))
#line 5085 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5085 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = tc_numeric_result_kind(ak, bk);

#line 5085 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;
}
else
#line 5086 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(18);
}
else
#line 5093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_value[id]==OP_MUL)||(node_value[id]==OP_DIV))||(node_value[id]==OP_MOD)))
#line 5091 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_is_numeric_kind(ak)==0)||(tc_is_numeric_kind(bk)==0)))
#line 5088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(18);
else
#line 5088 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5089 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_kind = tc_numeric_result_kind(ak, bk);

#line 5090 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_name = 0;
}
else
#line 5093 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5092 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(18);
}

#line 5094 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_result_type = tc_type_node_from_summary(tc_kind, tc_name, tc_elem_kind, tc_elem_name);

#line 5095 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux[id] = tc_result_type;

#line 5096 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
}
else
#line 5097 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5098 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(19);
}

#line 5108 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_function(int name)
#line 5108 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5102 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[tc_root];

#line 5106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 5106 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((node_kind[item]==N_FUNC)||(node_kind[item]==N_GENERIC_FUNC))||(node_kind[item]==N_EXTERN))&&(node_value[item]==name)))
#line 5104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return item;
else
#line 5104 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5105 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 5107 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 5120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_function_ctx(int name, int ns)
#line 5120 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5111 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int exact = tc_find_function(name);

#line 5112 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((exact!=0))
#line 5112 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return exact;
else
#line 5112 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5113 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ns==0))
#line 5113 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 5113 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5114 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[tc_root];

#line 5118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 5118 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5116 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((((node_kind[item]==N_FUNC)||(node_kind[item]==N_GENERIC_FUNC))||(node_kind[item]==N_EXTERN))&&(node_scope[item]==ns))&&(sym_suffix_equal(node_value[item], name)==1)))
#line 5116 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return item;
else
#line 5116 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5117 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 5119 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 5132 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_enum_value(int name)
#line 5132 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5123 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[tc_root];

#line 5130 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 5130 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5128 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_ENUM))
#line 5128 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5126 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[item];

#line 5127 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((f!=0))
#line 5127 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5127 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[f]==name))
#line 5127 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_value[item];
else
#line 5127 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5127 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];
}
}
else
#line 5128 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5129 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 5131 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 5150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_find_enum_variant(int name)
#line 5150 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5135 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_variant_enum = 0;

#line 5136 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_variant_member = 0;

#line 5137 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[tc_root];

#line 5148 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 5148 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_ENUM))
#line 5146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5140 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[item];

#line 5145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((f!=0))
#line 5145 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5142 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int qualified = sym_qualified(node_value[item], node_a[f]);

#line 5143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((qualified==name))
#line 5143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_variant_enum = node_value[item];

#line 5143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_variant_member = f;

#line 5143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 5143 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5144 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];
}
}
else
#line 5146 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5147 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 5149 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 5157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_match_enum_decl(int ty)
#line 5157 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5153 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((ty==0)||(node_kind[ty]!=TY_NAMED)))
#line 5153 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 5153 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5154 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int decl = tc_find_enum_ctx(node_value[ty], node_scope[ty]);

#line 5155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((decl==0))
#line 5155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
decl = tc_find_enum(node_value[ty]);
else
#line 5155 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5156 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return decl;
}

#line 5169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_match_variant_member(int decl, int name)
#line 5169 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5160 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((decl==0)||(node_kind[decl]!=N_ENUM)))
#line 5160 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 5160 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5161 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = node_a[decl];

#line 5167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((field!=0))
#line 5167 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5163 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[field]==name))
#line 5163 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return field;
else
#line 5163 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5164 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int qualified = sym_qualified(node_value[decl], node_a[field]);

#line 5165 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((qualified==name)||(sym_suffix_equal(qualified, name)==1)))
#line 5165 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return field;
else
#line 5165 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5166 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
field = node_next[field];
}

#line 5168 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 5178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_match_seen_variant(int head, int member)
#line 5178 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5172 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arm = head;

#line 5176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((arm!=0))
#line 5176 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5174 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_aux[arm]==member))
#line 5174 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 5174 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5175 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arm = node_next[arm];
}

#line 5177 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 5191 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_match_check_arm_bindings(int variant, int bindings)
#line 5191 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5181 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field = node_b[variant];

#line 5182 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int binding = bindings;

#line 5189 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((field!=0)&&(binding!=0)))
#line 5189 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5184 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[field]);

#line 5185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bk = tc_kind;

#line 5185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bn = tc_name;

#line 5185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int bek = tc_elem_kind;

#line 5185 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ben = tc_elem_name;

#line 5186 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_add_var(node_value[binding], bk, bn, bek, ben, node_b[field]);

#line 5187 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
binding = node_next[binding];

#line 5188 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
field = node_next[field];
}

#line 5190 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((field!=0)||(binding!=0)))
#line 5190 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(50);
else
#line 5190 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 5222 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_emit_field_type(int id)
#line 5222 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((id==0)||(node_kind[id]!=N_FIELD_ACCESS)))
#line 5194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 5194 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5195 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int base_ty = tc_emit_arg_type(node_a[id]);

#line 5196 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((base_ty==0))
#line 5196 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 5196 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5197 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
base_ty = gen_substitute_type(base_ty);

#line 5203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[base_ty]==TY_VARIANT))
#line 5203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5199 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int variant_item = node_value[base_ty];

#line 5200 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int variant_field = node_b[variant_item];

#line 5201 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((variant_field!=0))
#line 5201 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5201 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[variant_field]==node_value[id]))
#line 5201 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_substitute_type(node_b[variant_field]);
else
#line 5201 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5201 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
variant_field = node_next[variant_field];
}

#line 5202 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 5203 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5204 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int struct_name = 0;

#line 5205 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int args = 0;

#line 5208 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[base_ty]==TY_GENERIC))
#line 5206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
struct_name = node_value[base_ty];

#line 5206 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
args = node_a[base_ty];
}
else
#line 5208 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[base_ty]==TY_NAMED))
#line 5207 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
struct_name = node_value[base_ty];
else
#line 5208 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;

#line 5209 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int decl = tc_find_struct(struct_name);

#line 5210 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((decl==0))
#line 5210 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 5210 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[decl]==N_GENERIC_STRUCT))
#line 5215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gp = node_c[decl];

#line 5212 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ga = args;

#line 5213 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_bind_clear();

#line 5214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((gp!=0)&&(ga!=0)))
#line 5214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_bind_add(node_a[gp], ga)==0))
#line 5214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 5214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gp = node_next[gp];

#line 5214 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ga = node_next[ga];
}
}
else
#line 5215 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5216 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[decl];

#line 5220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((f!=0))
#line 5220 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[f]==node_value[id]))
#line 5218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_substitute_type(node_b[f]);
else
#line 5218 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5219 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];
}

#line 5221 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 5249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_emit_arg_type(int id)
#line 5249 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5225 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((id==0))
#line 5225 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
else
#line 5225 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[id]==N_FIELD_ACCESS)&&(node_aux[id]!=0))&&(node_kind[node_aux[id]]==TY_VARIANT)))
#line 5226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_aux[id];
else
#line 5226 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_FIELD_ACCESS))
#line 5230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5228 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int field_ty = tc_emit_field_type(id);

#line 5229 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((field_ty!=0))
#line 5229 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return field_ty;
else
#line 5229 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5230 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5235 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_VAR))
#line 5235 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5232 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int formal_type = gen_active_param_type(node_value[id]);

#line 5233 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((formal_type!=0))
#line 5233 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_substitute_type(formal_type);
else
#line 5233 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_aux[id]!=0))
#line 5234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_aux[id];
else
#line 5234 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5235 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5236 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_STRING))
#line 5236 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_STRING, 0, 0, 0, 0, 0);
else
#line 5236 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5237 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_INT))
#line 5237 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_INT, 0, 0, 0, 0, 0);
else
#line 5237 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5238 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_BOOL))
#line 5238 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_BOOL, 0, 0, 0, 0, 0);
else
#line 5238 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5239 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_CHAR))
#line 5239 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_CHAR, 0, 0, 0, 0, 0);
else
#line 5239 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_FLOAT))
#line 5240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_aux[id]==TY_FLOAT))
#line 5240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_FLOAT, 0, 0, 0, 0, 0);
else
#line 5240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_DOUBLE, 0, 0, 0, 0, 0);
}
else
#line 5240 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5241 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_NULL))
#line 5241 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ast_node(TY_PTR, ast_node(TY_VOID, 0, 0, 0, 0, 0), 0, 0, 0, 0);
else
#line 5241 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5246 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[id]==N_TUPLE))
#line 5246 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_aux[id]!=0))
#line 5243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return gen_substitute_type(node_aux[id]);
else
#line 5243 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5244 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(id);

#line 5245 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_result_type;
}
else
#line 5246 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5247 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ek = gen_expr_kind(id);

#line 5248 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_type_node_from_summary(ek, 0, tc_elem_kind, tc_elem_name);
}

#line 5292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_expr_kind_for_emit(int id)
#line 5292 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((id!=0)&&(node_kind[id]==N_CALL))&&(node_aux[id]!=0)))
#line 5252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_kind[node_aux[id]];
else
#line 5252 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((id!=0)&&(node_kind[id]==N_INDIRECT_CALL)))
#line 5256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5254 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(id);

#line 5255 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return tc_kind;
}
else
#line 5256 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5257 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = tc_find_function_ctx(node_value[id], node_scope[id]);

#line 5290 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((f!=0))
#line 5290 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[f]==N_GENERIC_FUNC))
#line 5288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5260 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int actual = 0;

#line 5260 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = node_a[id];

#line 5265 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((a!=0))
#line 5265 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5262 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int q = tc_emit_arg_type(a);

#line 5263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((q!=0))
#line 5263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((actual==0))
#line 5263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
actual = q;
else
#line 5263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
actual = ast_link(actual, q);
}
else
#line 5263 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5264 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}

#line 5266 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int saved_count = gen_bind_count;

#line 5267 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ensure_gen_bind((saved_count+saved_count));

#line 5268 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int save_i = 0;

#line 5273 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((save_i<saved_count))
#line 5273 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5270 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[(saved_count+save_i)] = gen_bind_name[save_i];

#line 5271 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[(saved_count+save_i)] = gen_bind_type[save_i];

#line 5272 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
save_i = (save_i+1);
}

#line 5274 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_decl(f, actual);

#line 5275 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ret = gen_substitute_type(node_b[f]);

#line 5276 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int result_kind = TY_INT;

#line 5277 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ret!=0))
#line 5277 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5277 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
result_kind = node_kind[ret];
}
else
#line 5277 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5278 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_clear();

#line 5279 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int restore_i = 0;

#line 5284 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((restore_i<saved_count))
#line 5284 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5281 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_name[restore_i] = gen_bind_name[(saved_count+restore_i)];

#line 5282 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_type[restore_i] = gen_bind_type[(saved_count+restore_i)];

#line 5283 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
restore_i = (restore_i+1);
}

#line 5285 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_bind_count = saved_count;

#line 5286 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((result_kind==TY_PARAM))
#line 5286 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5286 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
}
else
#line 5286 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5287 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return result_kind;
}
else
#line 5288 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5289 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_b[f]!=0))
#line 5289 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5289 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return node_kind[node_b[f]];
}
else
#line 5289 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5290 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5291 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return TY_INT;
}

#line 5418 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_stmt(int id, int expected_kind, int expected_name)
#line 5418 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5295 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((id!=0))
#line 5295 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_error_pos = node_pos[id];
else
#line 5295 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5296 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_ok==0)||(id==0)))
#line 5296 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 5296 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5297 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int k = node_kind[id];

#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_CONST))
#line 5305 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[id]);

#line 5299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ck = tc_kind;

#line 5299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int cn = tc_name;

#line 5299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ce = tc_elem_kind;

#line 5299 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int cen = tc_elem_name;

#line 5300 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(node_c[id], ck);

#line 5301 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_c[id]);

#line 5302 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_literal_fits(node_c[id], ck)==0))
#line 5302 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(54);
else
#line 5302 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_same_full(ck, cn, ce, cen, tc_kind, tc_name, tc_elem_kind, tc_elem_name)==0))
#line 5303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[node_c[id]]!=N_INT)||(node_value[node_c[id]]!=0)))
#line 5303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[node_c[id]]!=N_NULL))
#line 5303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(30);
else
#line 5303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 5303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5303 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_add_var(node_a[id], ck, cn, ce, cen, node_b[id]);

#line 5304 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_type[node_a[id]] = (ck+100);
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_LET))
#line 5320 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[id]);

#line 5306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int dk = tc_kind;

#line 5306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int dn = tc_name;

#line 5306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int de = tc_elem_kind;

#line 5306 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int den = tc_elem_name;

#line 5307 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(node_c[id], dk);

#line 5308 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_c[id]);

#line 5309 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ek = tc_kind;

#line 5309 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int en = tc_name;

#line 5309 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ee = tc_elem_kind;

#line 5309 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int een = tc_elem_name;

#line 5309 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int rhs_borrow = tc_expr_borrow_source;

#line 5310 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((tc_is_owner_kind(dk)==1)&&(node_kind[node_c[id]]==N_VAR)))
#line 5310 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(40);
else
#line 5310 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_literal_fits(node_c[id], dk)==0))
#line 5312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(54);
else
#line 5312 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5315 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_same_full(dk, dn, de, den, ek, en, ee, een)==0))
#line 5315 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5314 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[node_c[id]]!=N_INT)||(node_value[node_c[id]]!=0))&&(node_kind[node_c[id]]!=N_NULL)))
#line 5314 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(20);
else
#line 5314 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5315 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((dk==TY_DYN_ARRAY)&&(node_kind[node_c[id]]==N_VAR)))
#line 5316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_move_value(node_c[id]);
else
#line 5316 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5317 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_add_var(node_a[id], dk, dn, de, den, node_b[id]);

#line 5318 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((dk==TY_DYN_ARRAY)||(tc_owned_initializer(node_c[id])==1)))
#line 5318 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_owned[tc_last_var_index] = 1;
else
#line 5318 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((dk==TY_PTR))
#line 5319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((rhs_borrow<0))
#line 5319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 5319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_record_borrow(tc_last_var_index, rhs_borrow);
}
else
#line 5319 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((k==N_ASSIGN)||(k==N_COMPOUND_ASSIGN)))
#line 5343 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5321 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[node_a[id]]==N_VAR)&&(sym_type[node_value[node_a[id]]]>100)))
#line 5321 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(31);
else
#line 5321 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 5322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int lk = tc_kind;

#line 5322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ln = tc_name;

#line 5322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int le = tc_elem_kind;

#line 5322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int len = tc_elem_name;

#line 5322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int lhs_borrow = tc_expr_borrow_source;

#line 5322 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int lhs_index = tc_last_var_index;

#line 5323 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_require_mutable(node_a[id]);

#line 5324 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(node_b[id], lk);

#line 5325 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_b[id]);

#line 5326 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int rhs_borrow_assign = tc_expr_borrow_source;

#line 5330 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_COMPOUND_ASSIGN))
#line 5330 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5328 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int combined = ast_node(N_BINOP, node_a[id], node_b[id], 0, node_value[id], 0);

#line 5329 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(combined);
}
else
#line 5330 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((lk==TY_DYN_ARRAY)&&(node_kind[node_b[id]]==N_VAR)))
#line 5331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(40);
else
#line 5331 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5332 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_literal_fits(node_b[id], lk)==0))
#line 5332 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(54);
else
#line 5332 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_same_full(lk, ln, le, len, tc_kind, tc_name, tc_elem_kind, tc_elem_name)==0))
#line 5333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[node_b[id]]!=N_INT)||(node_value[node_b[id]]!=0))&&(node_kind[node_b[id]]!=N_NULL)))
#line 5333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(21);
else
#line 5333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5333 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5337 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((tc_ok==1)&&(node_kind[node_a[id]]==N_VAR))&&(lk==TY_DYN_ARRAY)))
#line 5337 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5335 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_owned[lhs_index] = 1;

#line 5336 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_moved[lhs_index] = 0;
}
else
#line 5337 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((tc_ok==1)&&(node_kind[node_a[id]]==N_VAR))&&(lk==TY_PTR)))
#line 5342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((lhs_borrow<0))
#line 5339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 5339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((lhs_borrow<tc_var_count)&&(tc_var_borrow_count[lhs_borrow]>0)))
#line 5339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_borrow_count[lhs_borrow] = (tc_var_borrow_count[lhs_borrow]-1);
else
#line 5339 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5340 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_borrow_source[lhs_index] = (0-1);

#line 5341 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((rhs_borrow_assign<0))
#line 5341 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 5341 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_record_borrow(lhs_index, rhs_borrow_assign);
}
else
#line 5342 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_DEFER))
#line 5346 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5344 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 5345 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_kind!=TY_VOID))
#line 5345 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(46);
else
#line 5345 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_TUPLE_BIND))
#line 5366 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5347 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[id]);

#line 5348 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int declared_ty = tc_result_type;

#line 5349 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int declared_kind = tc_kind;

#line 5350 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_c[id]);

#line 5351 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int rhs_ty = tc_result_type;

#line 5365 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((declared_kind!=TY_TUPLE)||(tc_kind!=TY_TUPLE)))
#line 5352 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(52);
else
#line 5365 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((declared_ty==0)||(rhs_ty==0))||(tc_type_equal(declared_ty, rhs_ty)==0)))
#line 5353 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(20);
else
#line 5365 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5355 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int elem = node_a[declared_ty];

#line 5356 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int binding = node_a[id];

#line 5363 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((elem!=0)&&(binding!=0)))
#line 5363 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5358 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(elem);

#line 5359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ek = tc_kind;

#line 5359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int en = tc_name;

#line 5359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int eek = tc_elem_kind;

#line 5359 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int een = tc_elem_name;

#line 5360 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_add_var(node_value[binding], ek, en, eek, een, elem);

#line 5361 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
elem = node_next[elem];

#line 5362 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
binding = node_next[binding];
}

#line 5364 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((elem!=0)||(binding!=0)))
#line 5364 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(53);
else
#line 5364 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_MATCH))
#line 5394 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5367 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 5368 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int subject_kind = tc_kind;

#line 5369 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int subject_type = tc_result_type;

#line 5370 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int enum_decl = 0;

#line 5371 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((subject_kind==TY_NAMED)&&(subject_type!=0)))
#line 5371 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
enum_decl = tc_match_enum_decl(subject_type);
else
#line 5371 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5393 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((enum_decl==0))
#line 5372 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(47);
else
#line 5393 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5374 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int arm = node_b[id];

#line 5387 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((arm!=0))
#line 5387 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5376 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int member = tc_match_variant_member(enum_decl, node_value[arm]);

#line 5385 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((member==0))
#line 5377 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(48);
else
#line 5385 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5379 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_match_seen_variant(node_b[id], member)==1))
#line 5379 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(49);
else
#line 5379 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5380 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_aux[arm] = member;

#line 5381 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_enter_scope();

#line 5382 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_match_check_arm_bindings(member, node_a[arm]);

#line 5383 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_stmt(node_b[arm], expected_kind, expected_name);

#line 5384 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_leave_scope();
}

#line 5386 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
arm = node_next[arm];
}

#line 5388 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int variant = node_a[enum_decl];

#line 5392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((variant!=0))
#line 5392 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5390 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_match_seen_variant(node_b[id], variant)==0))
#line 5390 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(51);
else
#line 5390 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5391 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
variant = node_next[variant];
}
}
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_PRINT))
#line 5394 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_EXPR))
#line 5395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 5395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[node_a[id]]==N_CALL))
#line 5395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_consume_call(node_a[id]);
else
#line 5395 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_RETURN))
#line 5399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[id]==0))
#line 5397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((expected_kind!=TY_VOID))
#line 5397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(22);
else
#line 5397 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(node_a[id], expected_kind);

#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int return_borrow = tc_expr_borrow_source;

#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_literal_fits(node_a[id], expected_kind)==0))
#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(54);
else
#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((expected_kind==TY_PTR))
#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((return_borrow<0))
#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(38);
}
else
#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_same_full(expected_kind, expected_name, tc_expected_elem_kind, tc_expected_elem_name, tc_kind, tc_name, tc_elem_kind, tc_elem_name)==0))
#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[node_a[id]]!=N_NULL))
#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(23);
else
#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
else
#line 5398 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((k==N_BREAK)||(k==N_CONTINUE)))
#line 5399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_loop_depth==0))
#line 5399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(24);
else
#line 5399 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_BLOCK))
#line 5404 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5400 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_enter_scope();

#line 5401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int x = node_a[id];

#line 5401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((x!=0))
#line 5401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_stmt(x, expected_kind, expected_name);

#line 5401 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
x = node_next[x];
}

#line 5402 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_leave_scope();
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_IF))
#line 5407 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5405 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 5405 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((tc_is_numeric_kind(tc_kind)==0)&&(tc_kind!=TY_PTR))&&(tc_kind!=TY_FUN)))
#line 5405 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(25);
else
#line 5405 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5406 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_stmt(node_b[id], expected_kind, expected_name);

#line 5406 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_stmt(node_c[id], expected_kind, expected_name);
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_WHILE))
#line 5410 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5408 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_a[id]);

#line 5408 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((tc_is_numeric_kind(tc_kind)==0)&&(tc_kind!=TY_PTR))&&(tc_kind!=TY_FUN)))
#line 5408 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(26);
else
#line 5408 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5409 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_loop_depth = (tc_loop_depth+1);

#line 5409 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_stmt(node_b[id], expected_kind, expected_name);

#line 5409 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_loop_depth = (tc_loop_depth-1);
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((k==N_FOR))
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5411 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_enter_scope();

#line 5412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_a[id]!=0))
#line 5412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_stmt(node_a[id], expected_kind, expected_name);
else
#line 5412 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5413 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_b[id]);

#line 5413 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((tc_is_numeric_kind(tc_kind)==0)&&(tc_kind!=TY_PTR))&&(tc_kind!=TY_FUN)))
#line 5413 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(27);
else
#line 5413 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_loop_depth = (tc_loop_depth+1);

#line 5414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_stmt(node_c[id], expected_kind, expected_name);

#line 5414 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_loop_depth = (tc_loop_depth-1);

#line 5415 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_value[id]!=0))
#line 5415 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_stmt(node_value[id], expected_kind, expected_name);
else
#line 5415 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5416 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_leave_scope();
}
else
#line 5417 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 5424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_diag_line(int pos)
#line 5424 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5421 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 5421 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int line = 1;

#line 5422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((i<pos)&&(i<source_len)))
#line 5422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[i]==10))
#line 5422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
line = (line+1);
else
#line 5422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5422 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 5423 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return line;
}

#line 5429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_diag_col(int pos)
#line 5429 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5426 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 5426 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int col = 1;

#line 5427 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while(((i<pos)&&(i<source_len)))
#line 5427 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5427 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((source[i]==10))
#line 5427 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
col = 1;
else
#line 5427 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
col = (col+1);

#line 5427 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 5428 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return col;
}

#line 5471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void tc_diag()
#line 5471 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==3))
#line 5431 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: duplicate declaration");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==5))
#line 5432 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: unknown name");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==12))
#line 5433 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: invalid function arguments");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==13))
#line 5434 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: invalid argument count");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==14))
#line 5435 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: string concatenation requires strings");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==17))
#line 5436 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: invalid built-in argument type");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==18))
#line 5437 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: invalid arithmetic operands");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==20))
#line 5438 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: initializer type mismatch");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==21))
#line 5439 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: assignment type mismatch");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==23))
#line 5440 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: return type mismatch");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==28))
#line 5441 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: recursive struct definition");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==31))
#line 5442 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: assignment to const");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==33))
#line 5443 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: use after ownership move");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==34))
#line 5444 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: use after ownership move");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==35))
#line 5445 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: release requires an owned value");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==36))
#line 5446 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: array element type mismatch");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==37))
#line 5447 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: cannot mutate or move while borrowed");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==38))
#line 5448 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: borrowed reference escapes its owner");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==40))
#line 5449 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: owned value copy requires an explicit move");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==41))
#line 5450 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: unknown function");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==42))
#line 5451 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: cannot call non-function value");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==43))
#line 5452 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: function name is reserved by the C runtime");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==45))
#line 5453 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: array index out of bounds");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==46))
#line 5454 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: defer expression must return void");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==47))
#line 5455 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: match subject must be an enum");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==48))
#line 5456 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: unknown variant in match arm");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==49))
#line 5457 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: duplicate variant in match");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==50))
#line 5458 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: match arm payload arity mismatch");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==51))
#line 5459 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: match is not exhaustive");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==52))
#line 5460 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: tuple destructuring requires tuple type");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==53))
#line 5461 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: tuple binding count mismatch");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==54))
#line 5462 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: integer literal is out of range for its target type");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==44))
#line 5463 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: distinct functions collide after C mangling");
else
#line 5464 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "type error: invalid expression");

#line 5465 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "diagnostic.code");

#line 5466 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%d\n", tc_error_code);

#line 5467 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "diagnostic.line");

#line 5468 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%d\n", tc_diag_line(tc_error_pos));

#line 5469 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "diagnostic.column");

#line 5470 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%d\n", tc_diag_col(tc_error_pos));
}

#line 5489 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_check_function_symbols(int root)
#line 5489 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5473 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int a = node_a[root];

#line 5487 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((a!=0))
#line 5487 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[a]==N_FUNC)||(node_kind[a]==N_GENERIC_FUNC))||(node_kind[a]==N_EXTERN)))
#line 5485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5476 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int b = node_next[a];

#line 5484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((b!=0))
#line 5484 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((node_kind[b]==N_FUNC)||(node_kind[b]==N_GENERIC_FUNC))||(node_kind[b]==N_EXTERN))&&(node_value[a]!=node_value[b])))
#line 5482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5479 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ca = sym_c_symbol(node_value[a]);

#line 5480 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int cb = sym_c_symbol(node_value[b]);

#line 5481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((ca==cb))
#line 5481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_error_code = 44;

#line 5481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_error_pos = node_pos[b];

#line 5481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_ok = 0;

#line 5481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 5481 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5482 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5483 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
b = node_next[b];
}
}
else
#line 5485 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5486 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
a = node_next[a];
}

#line 5488 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 5493 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_reserved_function(int name)
#line 5493 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((bi_has_flag(name, BI_FLAG_RESERVED)==1))
#line 5491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
else
#line 5491 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5492 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}

#line 5515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int tc_program(int root)
#line 5515 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_root = root;

#line 5495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_ok = 1;

#line 5495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_error_code = 0;

#line 5495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_count = 0;

#line 5495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_scope_count = 0;

#line 5495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_path_count = 0;

#line 5495 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_loop_depth = 0;

#line 5496 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int collision_item = node_a[root];

#line 5500 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((collision_item!=0))
#line 5500 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((node_kind[collision_item]==N_FUNC)||(node_kind[collision_item]==N_GENERIC_FUNC))&&(tc_reserved_function(node_value[collision_item])==1)))
#line 5498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_error_code = 43;

#line 5498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_error_pos = node_pos[collision_item];

#line 5498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_ok = 0;

#line 5498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_diag();

#line 5498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 5498 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5499 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
collision_item = node_next[collision_item];
}

#line 5501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_check_function_symbols(root)==0))
#line 5501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_diag();

#line 5501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 5501 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5502 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_enter_scope();

#line 5503 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int item = node_a[root];

#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_STRUCT))
#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int f = node_a[item];

#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((f!=0))
#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_check_type(node_b[f]);

#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
f = node_next[f];
}

#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_cycle_struct(node_value[item])==1))
#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(28);
else
#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5504 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 5505 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_a[root];

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((node_kind[item]==N_GLOBAL)||(node_kind[item]==N_CONST)))
#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[item]);

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gk = tc_kind;

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gn = tc_name;

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ge = tc_elem_kind;

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int gen = tc_elem_name;

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_add_var(node_a[item], gk, gn, ge, gen, node_b[item]);

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_CONST))
#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
sym_type[node_a[item]] = (gk+100);
else
#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_mark_float_expr(node_c[item], gk);

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expr(node_c[item]);

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_literal_fits(node_c[item], gk)==0))
#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(54);
else
#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((((tc_same_full(gk, gn, ge, gen, tc_kind, tc_name, tc_elem_kind, tc_elem_name)==0)&&((node_kind[node_c[item]]!=N_INT)||(node_value[node_c[item]]!=0)))&&(node_kind[node_c[item]]!=N_NULL)))
#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_fail(29);
else
#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5506 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 5508 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_global_count = tc_var_count;

#line 5509 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_a[root];

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((item!=0))
#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((node_kind[item]==N_FUNC))
#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5510 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_count = tc_global_count;

#line 5510 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_scope_count = 1;

#line 5510 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_enter_scope();

#line 5510 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int p = node_c[item];

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((p!=0))
#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[p]);

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pk = tc_kind;

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pn = tc_name;

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pek = tc_elem_kind;

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pen = tc_elem_name;

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_add_var(node_a[p], pk, pn, pek, pen, node_b[p]);

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((pk==TY_DYN_ARRAY))
#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_var_owned[tc_last_var_index] = 1;
else
#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
p = node_next[p];
}

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_type_node(node_b[item]);

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expected_elem_kind = tc_elem_kind;

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_expected_elem_name = tc_elem_name;

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_stmt(node_a[item], tc_kind, tc_name);

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_leave_scope();
}
else
#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5511 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
item = node_next[item];
}

#line 5513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_ok==0))
#line 5513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
tc_diag();

#line 5513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 5513 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5514 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 5554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int pipeline_main(char* path)
#line 5554 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5518 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
load_tokens_from_file(path);

#line 5519 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
node_count = 1;

#line 5520 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_parse_mode = 1;

#line 5521 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int root = ast_program();

#line 5522 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ast_parse_mode = 0;

#line 5523 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
pipeline_root = root;

#line 5524 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int parsed = 1;

#line 5525 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((root<0))
#line 5525 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
parsed = 0;
else
#line 5525 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((input_peek()!=T_EOF))
#line 5526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
parsed = 0;
else
#line 5526 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5527 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((include_ok==0))
#line 5527 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
parsed = 0;
else
#line 5527 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((parsed==1))
#line 5530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5529 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_program(root)==0))
#line 5529 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
parsed = 0;
else
#line 5529 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5530 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((parsed==1))
#line 5540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5532 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_reset();

#line 5533 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_program(root);

#line 5534 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_reset();

#line 5535 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_program(root);

#line 5536 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int stable_count = code_count;

#line 5537 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
code_reset();

#line 5538 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_program(root);

#line 5539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((code_count!=stable_count))
#line 5539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
parsed = 0;
else
#line 5539 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5540 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((parsed==0))
#line 5551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((tc_error_code==0))
#line 5550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5543 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "parse error");

#line 5544 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "diagnostic.code");

#line 5545 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%d\n", 0);

#line 5546 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "diagnostic.line");

#line 5547 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%d\n", tc_diag_line(source_pos));

#line 5548 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%s\n", "diagnostic.column");

#line 5549 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
printf("%d\n", tc_diag_col(source_pos));
}
else
#line 5550 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5551 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((parsed==1))
#line 5552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 0;
}
else
#line 5552 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5553 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}

#line 5562 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_symbol(int*out, int id)
#line 5562 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5557 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 5561 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<sym_len[id]))
#line 5561 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5559 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, source[(sym_start[id]+i)]);

#line 5560 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}
}

#line 5568 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_string(int*out, int id)
#line 5568 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5565 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5566 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_symbol(out, id);

#line 5567 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);
}

#line 5578 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_print_prefix(int*out)
#line 5578 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5571 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "(");

#line 5572 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5573 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "%d");

#line 5574 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5575 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 110);

#line 5576 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5577 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ", ");
}

#line 5590 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_int_text(int*out, int value)
#line 5590 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5589 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value<0))
#line 5584 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5582 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 45);

#line 5583 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_int_text(out, (0-value));
}
else
#line 5589 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value<10))
#line 5586 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5585 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, (48+value));
}
else
#line 5589 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5587 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_int_text(out, (value/10));

#line 5588 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, (48+(value-((value/10)*10))));
}
}

#line 5603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_source_filename(int*out, int file_id)
#line 5603 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5593 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 5602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<source_file_name_len[file_id]))
#line 5602 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5595 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int c = source_file_name_text[(source_file_name_start[file_id]+i)];

#line 5600 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((c==34)||(c==92)))
#line 5596 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5596 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5596 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, c);
}
else
#line 5600 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==10))
#line 5597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5597 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 110);
}
else
#line 5600 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==13))
#line 5598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5598 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 114);
}
else
#line 5600 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((c==9))
#line 5599 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5599 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5599 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 116);
}
else
#line 5600 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, c);

#line 5601 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}
}

#line 5615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_source_line(int*out, int pos)
#line 5615 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5606 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((pos<0)||(pos>(source_len-1))))
#line 5606 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 5606 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5607 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int file_id = source_file_at[pos];

#line 5608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((file_id<1)||(file_id>(source_file_count-1))))
#line 5608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return;
else
#line 5608 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5609 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 10);

#line 5610 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "#line ");

#line 5611 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_int_text(out, source_line_at[pos]);

#line 5612 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, " \"");

#line 5613 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_source_filename(out, file_id);

#line 5614 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "\"\n");
}

#line 5739 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_c_token(int*out, int kind, int value)
#line 5739 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((emit_pending_space==1))
#line 5621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((((((kind==C_KW)||(kind==C_IDENT))||(kind==C_INT))||(kind==C_STRING))||(kind==C_RAW))||(kind==C_RAW_U64)))
#line 5619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 32);
else
#line 5619 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5620 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 0;
}
else
#line 5621 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==C_KW))
#line 5653 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==1))
#line 5623 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5623 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "int");

#line 5623 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==2))
#line 5624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "int");

#line 5624 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==3))
#line 5625 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5625 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "char*");

#line 5625 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==4))
#line 5626 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5626 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "void");

#line 5626 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==5))
#line 5627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "return");

#line 5627 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==6))
#line 5628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "if");

#line 5628 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==7))
#line 5629 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5629 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "else");

#line 5629 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==8))
#line 5630 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5630 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "while");

#line 5630 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==9))
#line 5631 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5631 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "break");

#line 5631 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==10))
#line 5632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "continue");

#line 5632 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==11))
#line 5633 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5633 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "for");

#line 5633 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==12))
#line 5634 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5634 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "struct");

#line 5634 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==13))
#line 5635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "enum");

#line 5635 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==14))
#line 5636 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5636 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "typedef");

#line 5636 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==15))
#line 5637 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5637 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "double");

#line 5637 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==16))
#line 5638 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5638 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "const");

#line 5638 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==18))
#line 5639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "float");

#line 5639 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==19))
#line 5640 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5640 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "long");

#line 5640 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==20))
#line 5641 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5641 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "long long");

#line 5641 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==21))
#line 5642 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5642 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "uint8_t");

#line 5642 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==22))
#line 5643 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5643 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "uint16_t");

#line 5643 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==23))
#line 5644 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5644 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "uint32_t");

#line 5644 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==24))
#line 5645 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5645 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "uint64_t");

#line 5645 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==25))
#line 5646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "int8_t");

#line 5646 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==26))
#line 5647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "int16_t");

#line 5647 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==27))
#line 5648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "int32_t");

#line 5648 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==28))
#line 5649 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5649 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "int64_t");

#line 5649 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==29))
#line 5650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "size_t");

#line 5650 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==30))
#line 5651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "union");

#line 5651 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==17))
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "char");

#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 1;
}
else
#line 5652 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==C_IDENT))
#line 5667 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1001)))
#line 5654 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "printf");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1002)))
#line 5655 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "runtime_string_concat");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1011)))
#line 5656 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "sizeof");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1012)))
#line 5657 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "len");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1013)))
#line 5658 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "cap");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1015)))
#line 5659 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "data");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1016)))
#line 5660 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "basalt_memory_alloc");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1017)))
#line 5661 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "basalt_memory_resize");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1018)))
#line 5662 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "basalt_memory_free");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1019)))
#line 5663 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "basalt_memory_alloc_aligned");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1020)))
#line 5664 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "_Alignas");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==(0-1021)))
#line 5665 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "f");
else
#line 5666 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_symbol(out, value);
}
else
#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==C_INT))
#line 5667 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_int_text(out, value);
else
#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==C_RAW))
#line 5668 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_symbol(out, value);
else
#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==C_RAW_U64))
#line 5669 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5669 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_symbol(out, value);

#line 5669 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "ULL");
}
else
#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==C_STRING))
#line 5670 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_string(out, value);
else
#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==C_OP))
#line 5701 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==1))
#line 5672 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "+");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==2))
#line 5673 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "-");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==3))
#line 5674 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "*");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==4))
#line 5675 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "/");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==5))
#line 5676 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "==");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==6))
#line 5677 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "!=");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==7))
#line 5678 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "<");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==8))
#line 5679 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ">");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==9))
#line 5680 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "&&");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==10))
#line 5681 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "||");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==11))
#line 5682 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "++");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==12))
#line 5683 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "&");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==13))
#line 5684 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "|");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==14))
#line 5685 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "^");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==15))
#line 5686 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "<<");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==16))
#line 5687 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ">>");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==17))
#line 5688 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "%");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==18))
#line 5689 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ":");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==19))
#line 5690 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "+=");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==20))
#line 5691 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "-=");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==21))
#line 5692 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "*=");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==22))
#line 5693 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "/=");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==23))
#line 5694 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "%=");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==24))
#line 5695 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "&=");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==25))
#line 5696 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "|=");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==26))
#line 5697 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "^=");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==27))
#line 5698 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "<<=");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==28))
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ">>=");
else
#line 5699 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==C_PUNCT))
#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==1))
#line 5702 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "*");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==2))
#line 5703 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "[");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==3))
#line 5704 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "]");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==4))
#line 5705 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "(");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==5))
#line 5706 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ")");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==6))
#line 5707 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "(");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==7))
#line 5708 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ", ");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==8))
#line 5709 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ")");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==9))
#line 5710 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "*");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==10))
#line 5711 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "&");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==11))
#line 5712 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, " = ");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==12))
#line 5713 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ";");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==13))
#line 5714 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "{\n");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==14))
#line 5715 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "}\n");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==15))
#line 5716 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_print_prefix(out);
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==16))
#line 5725 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5718 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 40);

#line 5719 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5720 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "%s");

#line 5721 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5722 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 110);

#line 5723 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5724 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ", ");
}
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==17))
#line 5725 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ".");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==20))
#line 5726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 40);

#line 5726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "%c");

#line 5726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 110);

#line 5726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5726 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ", ");
}
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==21))
#line 5727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 40);

#line 5727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "%g");

#line 5727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 110);

#line 5727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5727 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ", ");
}
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==26))
#line 5728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 40);

#line 5728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "%p");

#line 5728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 110);

#line 5728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5728 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ", ");
}
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==18))
#line 5729 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, " ");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==19))
#line 5730 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "{0}");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==22))
#line 5731 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 32);
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==23))
#line 5732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 40);

#line 5732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "%d");

#line 5732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 110);

#line 5732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5732 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ", ");
}
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==24))
#line 5733 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "{");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==25))
#line 5734 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "}");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==27))
#line 5735 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "->");
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==28))
#line 5736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 40);

#line 5736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "%ld");

#line 5736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 110);

#line 5736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5736 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ", ");
}
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((value==29))
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 40);

#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "%lld");

#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 92);

#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 110);

#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 34);

#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, ", ");
}
else
#line 5737 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((kind==C_NEWLINE))
#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, 10);
else
#line 5738 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}

#line 5788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_runtime(int*out)
#line 5788 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5742 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "#if defined(_WIN32)\n#include <direct.h>\n#else\n#define _POSIX_C_SOURCE 200809L\n#define _XOPEN_SOURCE 700\n#endif\n#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n#include <stdint.h>\n#include <stddef.h>\n#include <limits.h>\n#include <errno.h>\n#include <stdatomic.h>\n#include <threads.h>\n#if defined(__GNUC__) || defined(__clang__)\n#define BASALT_UNUSED __attribute__((unused))\n#else\n#define BASALT_UNUSED\n#endif\nstatic void basalt_panic(int code){(void)code;exit(2);}\nstatic size_t basalt_checked_bytes(int count,size_t elem_size){if(count<0)basalt_panic(1);if(elem_size!=0&&(size_t)count>(size_t)-1/elem_size)basalt_panic(1);return(size_t)count*elem_size;}\nstatic void* basalt_track(void*);static void basalt_release(void*);\n");

#line 5743 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "typedef struct basalt_atomic_int { atomic_int value; } basalt_atomic_int;\nstatic BASALT_UNUSED void* basalt_atomic_make(int initial){basalt_atomic_int*a=(basalt_atomic_int*)calloc(1,sizeof(*a));if(!a)basalt_panic(5);atomic_init(&a->value,initial);return basalt_track(a);}\nstatic BASALT_UNUSED int basalt_atomic_load(void*p){basalt_atomic_int*a=(basalt_atomic_int*)p;if(!a)basalt_panic(4);return atomic_load_explicit(&a->value,memory_order_acquire);}\nstatic BASALT_UNUSED void basalt_atomic_store(void*p,int value){basalt_atomic_int*a=(basalt_atomic_int*)p;if(!a)basalt_panic(4);atomic_store_explicit(&a->value,value,memory_order_release);}\nstatic BASALT_UNUSED int basalt_atomic_fetch_add(void*p,int delta){basalt_atomic_int*a=(basalt_atomic_int*)p;if(!a)basalt_panic(4);return atomic_fetch_add_explicit(&a->value,delta,memory_order_acq_rel);}\nstatic BASALT_UNUSED int basalt_atomic_compare_exchange(void*p,int expected,int desired){basalt_atomic_int*a=(basalt_atomic_int*)p;int old;if(!a)basalt_panic(4);old=expected;return atomic_compare_exchange_strong_explicit(&a->value,&old,desired,memory_order_acq_rel,memory_order_acquire);}\nstatic BASALT_UNUSED void basalt_atomic_free(void*p){basalt_release(p);}\ntypedef struct basalt_channel { _Atomic size_t head; _Atomic size_t tail; atomic_int closed; size_t capacity; int data[]; } basalt_channel;\nstatic BASALT_UNUSED void* basalt_channel_make(int requested){size_t cap=2;size_t bytes;basalt_channel*c;if(requested<1||requested>1073741824)basalt_panic(7);while(cap<(size_t)requested){if(cap>(size_t)-1/2)basalt_panic(1);cap*=2;}if(cap>(size_t)-1/sizeof(int))basalt_panic(1);bytes=sizeof(*c)+cap*sizeof(int);if(bytes<sizeof(*c))basalt_panic(1);c=(basalt_channel*)calloc(1,bytes);if(!c)basalt_panic(5);c->capacity=cap;atomic_init(&c->head,0);atomic_init(&c->tail,0);atomic_init(&c->closed,0);return basalt_track(c);}\nstatic BASALT_UNUSED int basalt_channel_send(void*p,int value){basalt_channel*c=(basalt_channel*)p;size_t head,tail;if(!c)basalt_panic(4);if(atomic_load_explicit(&c->closed,memory_order_acquire)!=0)return -1;head=atomic_load_explicit(&c->head,memory_order_relaxed);tail=atomic_load_explicit(&c->tail,memory_order_acquire);if(head-tail>=c->capacity)return 0;c->data[head&(c->capacity-1)]=value;atomic_store_explicit(&c->head,head+1,memory_order_release);return 1;}\nstatic BASALT_UNUSED int basalt_channel_recv(void*p,int*out){basalt_channel*c=(basalt_channel*)p;size_t head,tail;if(!c||!out)basalt_panic(4);tail=atomic_load_explicit(&c->tail,memory_order_relaxed);head=atomic_load_explicit(&c->head,memory_order_acquire);if(tail==head){if(atomic_load_explicit(&c->closed,memory_order_acquire)!=0)return -1;return 0;}*out=c->data[tail&(c->capacity-1)];atomic_store_explicit(&c->tail,tail+1,memory_order_release);return 1;}\nstatic BASALT_UNUSED void basalt_channel_close(void*p){basalt_channel*c=(basalt_channel*)p;if(!c)basalt_panic(4);atomic_store_explicit(&c->closed,1,memory_order_release);}\nstatic BASALT_UNUSED void basalt_channel_free(void*p){basalt_release(p);}\ntypedef struct basalt_thread_handle { thrd_t thread; } basalt_thread_handle;\nstatic BASALT_UNUSED void* basalt_thread_spawn(int(*entry)(void*),void*arg){basalt_thread_handle*h=(basalt_thread_handle*)calloc(1,sizeof(*h));if(!h)basalt_panic(5);if(thrd_create(&h->thread,entry,arg)!=thrd_success){free(h);return NULL;}return basalt_track(h);}\nstatic BASALT_UNUSED int basalt_thread_join(void*p){basalt_thread_handle*h=(basalt_thread_handle*)p;int result;if(!h)basalt_panic(4);if(thrd_join(h->thread,&result)!=thrd_success)basalt_panic(8);basalt_release(h);return result;}\nstatic BASALT_UNUSED void basalt_thread_yield(void){thrd_yield();}\n");

#line 5744 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static char** basalt_inc_active=NULL;static size_t basalt_inc_active_n=0,basalt_inc_active_cap=0;static char** basalt_inc_loaded=NULL;static size_t basalt_inc_loaded_n=0,basalt_inc_loaded_cap=0;static int basalt_inc_status=0;\n");

#line 5745 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int basalt_inc_eq(const char*a,const char*b){return strcmp(a,b)==0;}\n");

#line 5746 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED size_t basalt_inc_find(char**v,size_t n,const char*p){size_t i;for(i=0;i<n;i++)if(basalt_inc_eq(v[i],p))return i;return (size_t)-1;}\n");

#line 5747 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void basalt_inc_add(char***vp,size_t*np,size_t*cp,char*p){size_t c;char**q;if(*np==*cp){c=*cp?*cp*2:16;q=(char**)realloc(*vp,c*sizeof(char*));if(!q)exit(2);*vp=q;*cp=c;}(*vp)[(*np)++]=p;}\n");

#line 5748 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED char* basalt_inc_strdup(const char*p){size_t n=strlen(p);char*q=(char*)malloc(n+1);if(!q)exit(2);memcpy(q,p,n+1);return(char*)basalt_track(q);}\n");

#line 5749 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED char* basalt_inc_realpath(const char*p){if(p&&p[0]==0&&basalt_inc_active_n)return basalt_inc_active[basalt_inc_active_n-1];\n#if defined(_WIN32)\nchar*q=_fullpath(NULL,p,0);if(q)return(char*)basalt_track(q);\n#else\nchar*q=realpath(p,NULL);if(q)return(char*)basalt_track(q);\n#endif\nreturn basalt_inc_strdup(p);}\n");

#line 5750 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int basalt_inc_begin(char*p){if(basalt_inc_find(basalt_inc_active,basalt_inc_active_n,p)!=(size_t)-1){basalt_inc_status=1;return 0;}if(basalt_inc_find(basalt_inc_loaded,basalt_inc_loaded_n,p)!=(size_t)-1){basalt_inc_status=2;return 0;}basalt_inc_add(&basalt_inc_active,&basalt_inc_active_n,&basalt_inc_active_cap,p);basalt_inc_status=0;return 1;}\n");

#line 5751 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void basalt_include_close(void){if(basalt_inc_active_n){char*p=basalt_inc_active[--basalt_inc_active_n];if(basalt_inc_find(basalt_inc_loaded,basalt_inc_loaded_n,p)==(size_t)-1)basalt_inc_add(&basalt_inc_loaded,&basalt_inc_loaded_n,&basalt_inc_loaded_cap,p);}}\n");

#line 5752 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED char* basalt_inc_join(const char*base,const char*raw){const char*s=strrchr(base,'/');size_t n=s?(size_t)(s-base+1):0;size_t m=strlen(raw);char*q=(char*)malloc(n+m+1);if(!q)exit(2);if(n)memcpy(q,base,n);memcpy(q+n,raw,m+1);return(char*)basalt_track(q);}\n");

#line 5753 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int basalt_include_line_mode(int*line,int n){int i=0,j;while(i<n&&(line[i]==' '||line[i]==9))i++;if(i+7<=n&&line[i]=='i'&&line[i+1]=='n'&&line[i+2]=='c'&&line[i+3]=='l'&&line[i+4]=='u'&&line[i+5]=='d'&&line[i+6]=='e'){j=i+7;while(j<n&&(line[j]==' '||line[j]==9))j++;if(j<n&&line[j]==34)return 1;}if(i+8<=n&&line[i]=='i'&&line[i+1]=='n'&&line[i+2]=='c'&&line[i+3]=='l'&&line[i+4]=='u'&&line[i+5]=='d'&&line[i+6]=='e'&&line[i+7]=='c'){j=i+8;while(j<n&&(line[j]==' '||line[j]==9))j++;if(j<n&&line[j]==34)return 2;}return 0;}\n");

#line 5754 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void* basalt_include_open_root(const char*path){char*p=basalt_inc_realpath(path);FILE*f;if(!basalt_inc_begin(p))return NULL;f=fopen(p,(const char[]){114,0});if(!f){basalt_inc_status=3;basalt_include_close();return NULL;}return(void*)f;}\n");

#line 5755 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void* basalt_include_open_line(int*line,int n,int mode){int i=0,a,b,j;char*raw,*joined,*canon;FILE*f;(void)mode;while(i<n&&line[i]!=34)i++;if(i>=n)return NULL;a=++i;while(i<n&&line[i]!=34)i++;if(i>=n)return NULL;b=i;raw=(char*)malloc((size_t)(b-a)+1);if(!raw)exit(2);{int k;for(k=0;k<b-a;k++)raw[k]=(char)line[a+k];}raw[b-a]=0;raw=(char*)basalt_track(raw);j=i+1;while(j<n&&(line[j]==32||line[j]==9))j++;if(j<n&&line[j]==59)j++;while(j<n&&(line[j]==32||line[j]==9))j++;if(j!=n)return NULL;joined=basalt_inc_join(basalt_inc_active[basalt_inc_active_n-1],raw);canon=basalt_inc_realpath(joined);if(!basalt_inc_begin(canon))return NULL;f=fopen(canon,(const char[]){114,0});if(!f){basalt_inc_status=3;basalt_include_close();return NULL;}return(void*)f;}\n");

#line 5756 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int basalt_include_last_status(void){return basalt_inc_status;}\n");

#line 5757 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void basalt_include_reset_session(void){basalt_inc_active_n=0;basalt_inc_loaded_n=0;basalt_inc_status=0;}\n");

#line 5758 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void* open_file(const char* p,const char* m){return (void*)fopen(p,m);}\n");

#line 5759 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int read_char(void* h){int c=fgetc((FILE*)h);return c==EOF?-1:c;}\n");

#line 5760 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int close_file(void* h){return fclose((FILE*)h);}\n");

#line 5761 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int write_char(void* h,int c){return fputc(c,(FILE*)h);}\n");

#line 5762 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int write_string(void* h,const char* s){return fputs(s,(FILE*)h);}\n");

#line 5763 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static void** basalt_live=NULL;static size_t basalt_live_n=0,basalt_live_cap=0;\n");

#line 5764 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED size_t basalt_find(void* p){size_t i;for(i=0;i<basalt_live_n;i++)if(basalt_live[i]==p)return i;return (size_t)-1;}\n");

#line 5765 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void basalt_validate(void){size_t i,j;for(i=0;i<basalt_live_n;i++){if(!basalt_live[i])basalt_panic(2);for(j=i+1;j<basalt_live_n;j++)if(basalt_live[i]==basalt_live[j])basalt_panic(2);}}\n");

#line 5766 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void basalt_cleanup(void){size_t i;basalt_validate();for(i=0;i<basalt_live_n;i++)free(basalt_live[i]);free(basalt_live);basalt_live=NULL;basalt_live_n=basalt_live_cap=0;}\n");

#line 5767 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void* basalt_track(void* p){size_t c;void**q;if(!p)return NULL;if(basalt_find(p)!=(size_t)-1)basalt_panic(2);if(basalt_live_n==basalt_live_cap){if(basalt_live_cap>(size_t)-1/2)c=(size_t)-1;else c=basalt_live_cap?basalt_live_cap*2:32;if(c>(size_t)-1/sizeof(void*))basalt_panic(2);q=(void**)realloc(basalt_live,c*sizeof(void*));if(!q)basalt_panic(2);basalt_live=q;basalt_live_cap=c;}basalt_live[basalt_live_n++]=p;atexit(basalt_cleanup);return p;}\n");

#line 5768 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void basalt_release(void* p){size_t i;if(!p)return;i=basalt_find(p);if(i==(size_t)-1)basalt_panic(2);free(p);basalt_live[i]=basalt_live[--basalt_live_n];}\n");

#line 5769 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static int basalt_io_status=0;\n");

#line 5770 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int runtime_io_status(void){return basalt_io_status;}\n");

#line 5771 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED char* runtime_read_line(int max_len){size_t n=0;int c=EOF;char*p;if(max_len<2||max_len>1048576)basalt_panic(7);p=(char*)malloc((size_t)max_len);if(!p)basalt_panic(5);while(n+1<(size_t)max_len){c=fgetc(stdin);if(c==EOF)break;if(c=='\\n')break;p[n++]=(char)c;}p[n]=0;if(c!=EOF&&c!='\\n'&&n+1==(size_t)max_len){basalt_io_status=3;do{c=fgetc(stdin);}while(c!=EOF&&c!='\\n');}else if(c==EOF&&n==0)basalt_io_status=1;else basalt_io_status=0;return(char*)basalt_track(p);}\n");

#line 5772 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int runtime_read_int(int fallback){char buf[128];size_t n=0;int c=EOF;char*end;long v;while(n+1<sizeof(buf)){c=fgetc(stdin);if(c==EOF||c=='\\n')break;if(c!='\\r')buf[n++]=(char)c;}buf[n]=0;if(c!=EOF&&c!='\\n'&&n+1==sizeof(buf)){basalt_io_status=3;do{c=fgetc(stdin);}while(c!=EOF&&c!='\\n');return fallback;}if(c==EOF&&n==0){basalt_io_status=1;return fallback;}errno=0;v=strtol(buf,&end,10);while(*end==' '||*end=='\\t'||*end=='\\r')end++;if(end==buf||*end!=0){basalt_io_status=2;return fallback;}if(errno==ERANGE||v<(long)INT_MIN||v>(long)INT_MAX){basalt_io_status=4;return fallback;}basalt_io_status=0;return(int)v;}\n");

#line 5773 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void runtime_write_string(const char*s){if(!s)basalt_panic(4);if(fputs(s,stdout)==EOF||fflush(stdout)!=0)basalt_panic(8);}\n");

#line 5774 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void runtime_write_line(const char*s){if(!s)basalt_panic(4);if(fputs(s,stdout)==EOF||fputc('\\n',stdout)==EOF||fflush(stdout)!=0)basalt_panic(8);}\n");

#line 5775 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void runtime_write_int(int value){if(fprintf(stdout,\"%d\",value)<0||fflush(stdout)!=0)basalt_panic(8);}\n");

#line 5776 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void runtime_write_char(char value){if(fputc((unsigned char)value,stdout)==EOF||fflush(stdout)!=0)basalt_panic(8);}\n");

#line 5777 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void* basalt_memory_alloc(int count,size_t elem_size){size_t bytes=basalt_checked_bytes(count,elem_size);void*p=calloc(1,bytes?bytes:1);if(!p)basalt_panic(5);return basalt_track(p);}\n");

#line 5778 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void* basalt_memory_alloc_aligned(int count,int alignment,size_t elem_size){size_t bytes,rounded,a;void*p;if(count<0||alignment<1)basalt_panic(1);a=(size_t)alignment;if((a&(a-1))!=0)basalt_panic(1);if(a<sizeof(void*))a=sizeof(void*);bytes=basalt_checked_bytes(count,elem_size);if(bytes==0)bytes=1;if(bytes>(size_t)-1-(a-1))basalt_panic(1);rounded=(bytes+a-1)&~(a-1);p=aligned_alloc(a,rounded);if(!p)basalt_panic(5);return basalt_track(p);}\n");

#line 5779 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void* basalt_memory_resize(void* old,int old_count,int new_count,size_t elem_size){size_t slot=(size_t)-1;size_t old_bytes;size_t new_bytes;void*p;if(old_count<0||new_count<0||new_count<old_count)basalt_panic(1);if(old){slot=basalt_find(old);if(slot==(size_t)-1)basalt_panic(2);}old_bytes=basalt_checked_bytes(old_count,elem_size);new_bytes=basalt_checked_bytes(new_count,elem_size);p=realloc(old,new_bytes?new_bytes:1);if(!p)basalt_panic(6);if(slot==(size_t)-1)basalt_track(p);else basalt_live[slot]=p;if(new_bytes>old_bytes)memset((char*)p+old_bytes,0,new_bytes-old_bytes);return p;}\n");

#line 5780 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void basalt_memory_free(void*p){basalt_release(p);}\n");

#line 5781 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED char* runtime_string_concat(const char* a,const char* b){size_t na,nb,total;char* p;if(!a||!b)basalt_panic(4);na=strlen(a);nb=strlen(b);if(na>(size_t)-1-nb-1)basalt_panic(1);total=na+nb+1;p=(char*)malloc(total);if(!p)basalt_panic(5);memcpy(p,a,na);memcpy(p+na,b,nb);p[na+nb]=0;return(char*)basalt_track(p);}\n");

#line 5782 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int* alloc_ints(int n){int* p;if(n<0)basalt_panic(1);if(n<1)n=1;basalt_checked_bytes(n,sizeof(int));p=(int*)calloc((size_t)n,sizeof(int));if(!p)basalt_panic(5);return(int*)basalt_track(p);}\n");

#line 5783 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED void free_ints(int* p){basalt_release(p);}\n");

#line 5784 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "static BASALT_UNUSED int* grow_ints(int* p,int old,int n){size_t slot=(size_t)-1;int* q;if(old<0||n<0)basalt_panic(1);if(n<=old)return p;if(p){slot=basalt_find(p);if(slot==(size_t)-1)basalt_panic(2);}basalt_checked_bytes(n,sizeof(int));q=(int*)realloc(p,(size_t)n*sizeof(int));if(!q)basalt_panic(6);if(p)basalt_live[slot]=q;else basalt_track(q);memset(q+old,0,(size_t)(n-old)*sizeof(int));return q;}\n");

#line 5785 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "extern int* payload_int; extern int* payload_name; extern int* payload_string;\n");

#line 5786 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "extern int* code_kind; extern int* code_value; extern int* input_kind; extern int* input_value;\n");

#line 5787 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_string(out, "extern int* source; extern int* sym_start; extern int* sym_len; extern int* sym_hash; extern int* sym_kind; extern int* sym_type; extern int* sym_scope;\n\n");
}

#line 5808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
void emit_c_file(char* path)
#line 5808 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5791 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int*out = open_file(path, "w");

#line 5792 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_runtime(out);

#line 5793 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ci = 0;

#line 5794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((ci<c_source_len))
#line 5794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
write_char(out, c_source[ci]);

#line 5794 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
ci = (ci+1);
}

#line 5795 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int i = 0;

#line 5796 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int last_epoch = (0-1);

#line 5797 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 0;

#line 5806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
while((i<code_count))
#line 5806 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((code_epoch[i]!=last_epoch))
#line 5803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5800 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_pending_space = 0;

#line 5801 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_source_line(out, code_pos[i]);

#line 5802 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
last_epoch = code_epoch[i];
}
else
#line 5803 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5804 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_c_token(out, code_kind[i], code_value[i]);

#line 5805 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
i = (i+1);
}

#line 5807 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
close_file(out);
}

#line 5822 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int main(int argc, char**argv){

#line 5813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((argc<2))
#line 5813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5812 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return 1;
}
else
#line 5813 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5814 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
int ok = pipeline_main(argv[1]);

#line 5820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if(((argc>2)&&(ok==0)))
#line 5820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
if((pipeline_root>0))
#line 5819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{

#line 5817 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
gen_program(pipeline_root);

#line 5818 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
emit_c_file(argv[2]);
}
else
#line 5819 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}
}
else
#line 5820 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
{
}

#line 5821 "/home/ubuntu/ash_github_publish/src/bootstrap/basaltc.bsl"
return ok;
return 0;
}
