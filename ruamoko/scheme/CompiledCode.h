#ifndef __CompiledCode_h
#define __CompiledCode_h
#include "SchemeObject.h"
#include "Array.h"
#include "Instruction.h"
#include "Frame.h"
#include "SchemeString.h"

struct lineinfo_s {
    integer linenumber;
    String *sourcefile;
};
    
typedef struct lineinfo_s lineinfo_t;

@interface CompiledCode: SchemeObject
{
    Frame *literals;
    Array *instructions;
    Array *constants;
    instruction_t *code;
    lineinfo_t *lineinfo;
    integer minargs, size;
}
- (void) addInstruction: (Instruction *) inst;
- (integer) addConstant: (SchemeObject *) c;
- (void) compile;
- (instruction_t *) code;
- (lineinfo_t *) lineinfo;
- (Frame *) literals;
- (integer) minimumArguments;
- (void) minimumArguments: (integer) min;
    
@end

#endif //__CompiledCode_h
