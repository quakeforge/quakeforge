#include "QF/gib.h"

void GIB_AddInstruction (char *name, gib_func_t func);
gib_inst_t *GIB_Find_Instruction (char *name);
void GIB_Init_Instructions (void);
int GIB_Echo_f (void);
int GIB_Call_f (void);
int GIB_Return_f (void);
int GIB_Con_f (void);
int GIB_ListFetch_f (void);
int GIB_ExpandVars (char *source, char *buffer, int bufferlen);
int GIB_ExpandBackticks (char *source, char *buffer, int bufferlen);
