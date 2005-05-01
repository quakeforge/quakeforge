#ifndef __Compiler_h
#define __Compiler_h
#include "SchemeObject.h"
#include "CompiledCode.h"
#include "Symbol.h"
#include "Scope.h"

@interface Compiler: SchemeObject
{
    CompiledCode code;
    SchemeObject sexpr;
    Symbol lambdaSym, quoteSym;
    Scope scope;
}

+ (id) newWithLambda: (SchemeObject) xp scope: (Scope) sc;
- (id) initWithLambda: (SchemeObject) xp scope: (Scope) sc;
- (void) compile;
- (CompiledCode) code;

@end

#endif //__Compiler_h
