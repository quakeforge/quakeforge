#include "CompiledCode.h"
#include "Symbol.h"
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

- (void) addInstruction: (Instruction *) inst
{
    [inst line: [self line]];
    [inst source: [self source]];
    [inst offset: [instructions count]];
    if ([inst opcode] != LABEL) {
            [instructions addObject: inst];
    }
}

- (int) addConstant: (SchemeObject *) c
{
    local int number = [constants count];
    [constants addObject: c];
    return number;
}
    
- (void) compile
{
    local unsigned index;
    local Instruction *inst;
    literals = [Frame newWithSize: [constants count] link: nil];
    code = obj_malloc (@sizeof(instruction_t) * [instructions count]);
    lineinfo = obj_malloc(@sizeof(lineinfo_t) * [instructions count]);
    for (index = 0; index < [constants count]; index++) {
            [literals set: index to: (SchemeObject*) [constants objectAtIndex: index]];
    }
    for (index = 0; index < [instructions count]; index++) {
            inst = [instructions objectAtIndex: index];
            [inst emitStruct: code];
            lineinfo[index].linenumber = [inst line];
            lineinfo[index].sourcefile = symbol([inst source]);
            [lineinfo[index].sourcefile retain];
    }
    size = [instructions count];
    [instructions release];
    [constants release];
    instructions = constants = nil;
}

- (instruction_t *) code
{
    return code;
}

- (lineinfo_t *) lineinfo
{
    return lineinfo;
}

- (Frame*) literals
{
    return literals;
}

- (void) dealloc
{
    local Array *temp;
    
    if (instructions) {
            temp = instructions;
            instructions = nil;
            [temp release];
    }
    if (constants) {
            temp = constants;
            constants = nil;
            [temp release];
    }
       
    if (code) {
            obj_free (code);
    }

    if (lineinfo) {
            local int i;
            for (i = 0; i < size; i++) {
                    [lineinfo[i].sourcefile release];
            }
            obj_free (lineinfo);
    }
    [super dealloc];
}

- (int) minimumArguments
{
    return minargs;
}

- (void) minimumArguments: (int) min
{
    minargs = min;
}

@end
