#ifndef __Compiler_h
#define __Compiler_h
#include "SchemeObject.h"
#include "CompiledCode.h"
#include "Symbol.h"
#include "Scope.h"
#include "Error.h"

@interface Compiler: SchemeObject
{
    CompiledCode code;
    SchemeObject sexpr;
    Symbol lambdaSym, quoteSym;
    Scope scope;
    Error err;
}

+ (id) newWithLambda: (SchemeObject) xp scope: (Scope) sc;
- (id) initWithLambda: (SchemeObject) xp scope: (Scope) sc;
- (SchemeObject) compile;

@end

#endif //__Compiler_h
