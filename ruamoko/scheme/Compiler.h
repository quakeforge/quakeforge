#ifndef __Compiler_h
#define __Compiler_h
#include "SchemeObject.h"
#include "CompiledCode.h"
#include "Symbol.h"
#include "Scope.h"
#include "Error.h"

#define TAIL 1

@interface Compiler: SchemeObject
{
    CompiledCode *code;
    SchemeObject *sexpr;
    Scope *scope;
    Error *err;
}

+ (id) newWithLambda: (SchemeObject *) xp scope: (Scope *) sc;
- (id) initWithLambda: (SchemeObject *) xp scope: (Scope *) sc;
- (SchemeObject*) compile;

- (void) emitExpression: (SchemeObject *) expression flags: (int) fl;
- (void) emitLambda: (SchemeObject *) expression;
- (void) emitConstant: (SchemeObject *) expression;
- (void) emitApply: (SchemeObject *) expression flags: (int) fl;
@end

#endif //__Compiler_h
