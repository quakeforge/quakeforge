#include "Instruction.h"
#include "defs.h"

@implementation Instruction
+ (id) opcode: (opcode_e) oc
{
    return [[self alloc] initWithOpcode: oc operand: 0 label: nil];
}

+ (id) opcode: (opcode_e) oc operand: (integer) op
{
    return [[self alloc] initWithOpcode: oc operand: op label: nil];
}

+ (id) opcode: (opcode_e) oc label: (Instruction []) l
{
    return [[self alloc] initWithOpcode: oc operand: 0 label: l];
}

- (id) initWithOpcode: (opcode_e) oc operand: (integer) op label: (Instruction []) l
{
    self = [super init];
    opcode = oc;
    operand = op;
    label = l;
    return self;
}

- (void) offset: (integer) ofs
{
    offset = ofs;
}

- (integer) offset
{
    return offset;
}

- (opcode_e) opcode
{
    return opcode;
}

- (void) emitStruct: (instruction_t []) program
{
    program[offset].opcode = opcode;
    if (label) {
            program[offset].operand = [label offset];
    } else {
            program[offset].operand = operand;
    }
}

- (void) markReachable
{
    [label mark];
}

@end
