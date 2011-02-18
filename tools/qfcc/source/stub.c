#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "class.h"
#include "codespace.h"
#include "debug.h"
#include "def.h"
#include "defspace.h"
#include "emit.h"
#include "function.h"
#include "immediate.h"
#include "obj_file.h"
#include "options.h"
#include "qfcc.h"
#include "strpool.h"
#include "type.h"

struct dstring_s;
options_t options;
int num_linenos;
pr_lineno_t *linenos;
pr_info_t pr;
defspace_t *new_defspace (void) {return 0;}
string_t ReuseString (const char *str) {return 0;}
void encode_type (struct dstring_s *str, type_t *type) {}
codespace_t *codespace_new (void) {return 0;}
void codespace_addcode (codespace_t *codespace, struct dstatement_s *code, int size) {}
int function_parms (function_t *f, byte *parm_size) {return 0;}
pr_auxfunction_t *new_auxfunction (void) {return 0;}
ddef_t *new_local (void) {return 0;}
void def_to_ddef (def_t *def, ddef_t *ddef, int aux) {}
int strpool_addstr (strpool_t *strpool, const char *str) {return 0;}
strpool_t *strpool_new (void) {return 0;}
void strpool_delete (strpool_t *strpool) {}
strpool_t *strpool_build (const char *strings, int size) {return 0;}
