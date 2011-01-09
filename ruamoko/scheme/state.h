#ifndef __state_h
#define __state_h

#include "Instruction.h"
#include "Frame.h"
#include "CompiledCode.h"

@class Continuation;

struct state_s {
    instruction_t [] program;
    lineinfo_t [] lineinfo;
    integer pc;
    Frame []literals, env;
    SchemeObject []stack;
    Continuation []cont;
    Procedure []proc;
};

typedef struct state_s state_t;
#endif
