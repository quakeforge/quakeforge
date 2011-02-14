#ifndef __Scope_h
#define __Scope_h
#include "SchemeObject.h"
#include "Array.h"
#include "Symbol.h"

@interface Scope: SchemeObject
{
    Scope *outerScope;
    Array *names;
}
+ (id) newWithOuter: (Scope *) o;
- (id) initWithOuter: (Scope *) o;
- (integer) depthOf: (Symbol *) sym;
- (integer) indexOf: (Symbol *) sym;
- (void) addName: (Symbol *) sym;
- (Scope *) outer;
@end

#endif //__Scope_h
