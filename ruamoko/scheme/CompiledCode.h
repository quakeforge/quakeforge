#ifndef __CompiledCode_h
#define __CompiledCode_h
#include "SchemeObject.h"
#include "Array.h"
#include "Instruction.h"
#include "Frame.h"
#include "SchemeString.h"

struct lineinfo_s {
    int linenumber;
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
    int minargs, size;
}
- (void) addInstruction: (Instruction *) inst;
- (int) addConstant: (SchemeObject *) c;
- (void) compile;
- (instruction_t *) code;
- (lineinfo_t *) lineinfo;
- (Frame *) literals;
- (int) minimumArguments;
- (void) minimumArguments: (int) min;
    
@end

#endif //__CompiledCode_h
