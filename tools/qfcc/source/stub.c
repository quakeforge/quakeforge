#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "class.h"
#include "codespace.h"
#include "diagnostic.h"
#include "dot.h"
#include "debug.h"
#include "def.h"
#include "defspace.h"
#include "emit.h"
#include "expr.h"
#include "function.h"
#include "obj_file.h"
#include "obj_type.h"
#include "options.h"
#include "qfcc.h"
#include "strpool.h"
#include "struct.h"
#include "symtab.h"
#include "type.h"
#include "value.h"

struct dstring_s;
options_t options;
int num_linenos;
pr_lineno_t *linenos;
pr_info_t pr;
type_t type_Class;
type_t type_SEL;
type_t type_id;
__attribute__((const)) string_t ReuseString (const char *str) {return 0;}
__attribute__((const)) codespace_t *codespace_new (void) {return 0;}
void codespace_addcode (codespace_t *codespace, struct dstatement_s *code, int size) {}
__attribute__((const)) int function_parms (function_t *f, byte *parm_size) {return 0;}
void def_to_ddef (def_t *def, ddef_t *ddef, int aux) {}
__attribute__((noreturn)) void _internal_error (expr_t *e, const char *file, int line, const char *fmt, ...) {abort();}
__attribute__((const)) expr_t *_warning (expr_t *e, const char *file, int line, const char *fmt, ...) {return 0;}
__attribute__((const)) expr_t *_error (expr_t *e, const char *file, int line, const char *fmt, ...) {return 0;}
__attribute__((const)) symbol_t *make_structure (const char *name, int su, struct_def_t *defs, type_t *type) {return 0;}
__attribute__((const)) symbol_t *symtab_addsymbol (symtab_t *symtab, symbol_t *symbol) {return 0;}
__attribute__((const)) symbol_t *new_symbol_type (const char *name, type_t *type) {return 0;}
__attribute__((const)) def_t *qfo_encode_type (type_t *type) {return 0;}
__attribute__((const)) int obj_types_assignable (const type_t *dst, const type_t *src) {return 0;}
void print_protocollist (struct dstring_s *dstr, protocollist_t *protocollist) {}
int is_id (const type_t *type){return type->type;}
int is_SEL (const type_t *type){return type->type;}
int is_Class (const type_t *type){return type->type;}
int compare_protocols (protocollist_t *protos1, protocollist_t *protos2){return protos1->count - protos2->count;}
void dump_dot (const char *stage, void *data,
		          void (*dump_func) (void *data, const char *fname)){}
void dump_dot_type (void *_t, const char *filename){}
const char *strip_path(const char *p) { return p;}
const char *file_basename(const char *p, int keepdot) { return p;}
