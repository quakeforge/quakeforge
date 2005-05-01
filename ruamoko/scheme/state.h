#ifndef __state_h
#define __state_h

#include "Instruction.h"
#include "Frame.h"

struct state_s = {
    instruction_t [] program;
    integer pc;
    Frame literals;
    SchemeObject stack;
};

typedef struct state_s state_t;
#endif
