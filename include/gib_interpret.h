#include "QF/gib.h"

#define GIB_MAXCALLS 2048
#define GIB_MAXSUBARGS 256

extern char *gib_subargv[256];
extern int gib_subargc;

char *GIB_Argv(int i);
int GIB_Argc(void);
void GIB_Strip_Arg (char *arg);
int GIB_Execute_Block (char *block, int retflag);
int GIB_Execute_Inst (void);
int GIB_Execute_Sub (void);
int GIB_Interpret_Inst (char *inst);
int GIB_Run_Inst (char *inst);
int GIB_Run_Sub (gib_module_t *mod, gib_sub_t *sub);
