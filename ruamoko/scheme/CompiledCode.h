#ifndef __CompiledCode_h
#define __CompiledCode_h
#include "SchemeObject.h"
#include "Array.h"
#include "Instruction.h"
#include "Frame.h"

@interface CompiledCode: SchemeObject
{
    Frame literals;
    Array instructions;
    Array constants;
    instruction_t [] code;
}
- (void) addInstruction: (Instruction) inst;
- (integer) addConstant: (SchemeObject) c;
- (void) compile;
- (instruction_t []) code;
- (Frame) literals;
@end

#endif //__CompiledCode_h
