#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "tools/qfcc/include/class.h"
#include "tools/qfcc/include/codespace.h"
#include "tools/qfcc/include/diagnostic.h"
#include "tools/qfcc/include/dot.h"
#include "tools/qfcc/include/debug.h"
#include "tools/qfcc/include/def.h"
#include "tools/qfcc/include/defspace.h"
#include "tools/qfcc/include/emit.h"
#include "tools/qfcc/include/expr.h"
#include "tools/qfcc/include/function.h"
#include "tools/qfcc/include/obj_file.h"
#include "tools/qfcc/include/obj_type.h"
#include "tools/qfcc/include/options.h"
#include "tools/qfcc/include/qfcc.h"
#include "tools/qfcc/include/strpool.h"
#include "tools/qfcc/include/struct.h"
#include "tools/qfcc/include/symtab.h"
#include "tools/qfcc/include/type.h"
#include "tools/qfcc/include/value.h"

struct dstring_s;
options_t options;
int num_linenos;
pr_lineno_t *linenos;
pr_info_t pr;
type_t type_Class;
type_t type_SEL;
type_t type_id;
void algebra_print_type_str (struct dstring_s *str, const struct type_s *type);
void algebra_print_type_str (struct dstring_s *str, const struct type_s *type){}
void algebra_encode_type (struct dstring_s *encoding,
		                          const struct type_s *type);
void algebra_encode_type (struct dstring_s *encoding,
		                          const struct type_s *type){}
__attribute__((const)) int algebra_type_size (const struct type_s *type);
int algebra_type_size (const struct type_s *type){return 0;}
__attribute__((const)) int algebra_type_width (const struct type_s *type);
int algebra_type_width (const struct type_s *type){return 0;}
__attribute__((const)) int algebra_type_assignable (const type_t *dst, const type_t *src);
int algebra_type_assignable (const type_t *dst, const type_t *src){return 0;}
__attribute__((const)) int is_algebra (const type_t *type);
int is_algebra (const type_t *type){return 0;}
__attribute__((const)) int algebra_base_type (const type_t *type);
int algebra_base_type (const type_t *type){return 0;}

__attribute__((const)) pr_string_t ReuseString (const char *str) {return 0;}
__attribute__((const)) codespace_t *codespace_new (void) {return 0;}
void codespace_addcode (codespace_t *codespace, struct dstatement_s *code, int size) {}
__attribute__((const)) int function_parms (function_t *f, byte *parm_size) {return 0;}
void def_to_ddef (def_t *def, ddef_t *ddef, int aux) {}
__attribute__((noreturn)) void _internal_error (const expr_t *e, const char *file, int line, const char *func, const char *fmt, ...) {abort();}
__attribute__((const)) expr_t *_warning (expr_t *e, const char *file, int line, const char *func, const char *fmt, ...) {return 0;}
__attribute__((const)) expr_t *_error (expr_t *e, const char *file, int line, const char *func, const char *fmt, ...) {return 0;}
__attribute__((const)) symbol_t *make_structure (const char *name, int su, struct_def_t *defs, type_t *type) {return 0;}
__attribute__((const)) symbol_t *symtab_addsymbol (symtab_t *symtab, symbol_t *symbol) {return 0;}
__attribute__((const)) symbol_t *new_symbol_type (const char *name, type_t *type) {return 0;}
__attribute__((const)) def_t *qfo_encode_type (type_t *type, defspace_t *space) {return 0;}
__attribute__((const)) int obj_types_assignable (const type_t *dst, const type_t *src) {return 0;}
void print_protocollist (struct dstring_s *dstr, protocollist_t *protocollist) {}
int is_id (const type_t *type){return type->type;}
int is_SEL (const type_t *type){return 0;}
int is_Class (const type_t *type){return 0;}
int compare_protocols (protocollist_t *protos1, protocollist_t *protos2){return protos1->count - protos2->count;}
void dump_dot (const char *stage, void *data,
		          void (*dump_func) (void *data, const char *fname)){}
void dump_dot_type (void *_t, const char *filename){}
char *fubar;
const char *file_basename(const char *p, int keepdot) { return fubar;}
