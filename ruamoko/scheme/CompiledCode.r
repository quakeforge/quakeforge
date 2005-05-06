#include "CompiledCode.h"
#include "defs.h"

@implementation CompiledCode
- (id) init
{
    self = [super init];
    constants = [Array new];
    instructions = [Array new];
    return self;
}

- (void) markReachable
{
    [literals mark];
    if (constants)
            [constants makeObjectsPerformSelector: @selector(mark)];
    if (instructions)
            [instructions makeObjectsPerformSelector: @selector(mark)];
}

- (void) addInstruction: (Instruction) inst
{
    [inst offset: [instructions count]];
    if ([inst opcode] != LABEL) {
            [instructions addItem: inst];
    }
}

- (integer) addConstant: (SchemeObject) c
{
    local integer number = [constants count];
    [constants addItem: c];
    return number;
}
    
- (void) compile
{
    local integer index;
    local Instruction inst;
    literals = [Frame newWithSize: [constants count] link: NIL];
    code = obj_malloc (@sizeof(instruction_t) * [instructions count]);
    for (index = 0; index < [constants count]; index++) {
            [literals set: index to: (SchemeObject) [constants getItemAt: index]];
    }
    for (index = 0; index < [instructions count]; index++) {
            inst = [instructions getItemAt: index];
            [inst emitStruct: code];
    }
    [instructions release];
    [constants release];
    instructions = constants = NIL;
}

- (instruction_t []) code
{
    return code;
}

- (Frame) literals
{
    return literals;
}

- (void) dealloc
{
    local Array temp;
    
    if (instructions) {
            temp = instructions;
            instructions = NIL;
            [temp release];
    }
    if (constants) {
            temp = constants;
            constants = NIL;
            [temp release];
    }
       
    if (code) {
            obj_free (code);
    }
    [super dealloc];
}

@end
