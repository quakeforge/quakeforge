#ifndef __Instruction_h
#define __Instruction_h
#include "SchemeObject.h"

typedef enum {
    LABEL,
    PUSH,
    POP,
    MAKECLOSURE,
    MAKECONT,
    LOADENV,
    LOADLITS,
    MAKEENV,
    POPENV,
    GET,
    SET,
    SETREST,
    SETSTACK,
    GETLINK,
    GETGLOBAL,
    SETGLOBAL,
    CALL,
    RETURN,
    IFFALSE,
    GOTO
} opcode_e;

struct instruction_s {
    opcode_e opcode;
    int operand;
};

typedef struct instruction_s instruction_t;

@interface Instruction: SchemeObject
{
    opcode_e opcode;
    int operand, offset;
    Instruction *label;
}
+ (id) opcode: (opcode_e) oc;
+ (id) opcode: (opcode_e) oc operand: (int) op;
+ (id) opcode: (opcode_e) oc label: (Instruction *) l;
- (id) initWithOpcode: (opcode_e) oc operand: (int) op label: (Instruction *) l;
- (void) offset: (int) ofs;
- (int) offset;
- (opcode_e) opcode;
- (void) emitStruct: (instruction_t *) program;

@end

#endif //__Instruction_h
