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
    GET,
    SET,
    GETLINK,
    GETGLOBAL,
    CALL,
    RETURN
} opcode_e;

struct instruction_s {
    opcode_e opcode;
    integer operand;
};

typedef struct instruction_s instruction_t;

@interface Instruction: SchemeObject
{
    opcode_e opcode;
    integer operand, offset;
    Instruction label;
}
+ (id) opcode: (opcode_e) oc;
+ (id) opcode: (opcode_e) oc operand: (integer) op;
+ (id) opcode: (opcode_e) oc label: (Instruction) l;
- (id) initWithOpcode: (opcode_e) oc operand: (integer) op label: (Instruction) l;
- (void) offset: (integer) ofs;
- (integer) offset;
- (opcode_e) opcode;
- (void) emitStruct: (instruction_t []) program;

@end

#endif //__Instruction_h
