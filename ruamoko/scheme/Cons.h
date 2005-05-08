#ifndef __Cons_h
#define __Cons_h
#include "SchemeObject.h"

@interface Cons: SchemeObject
{
    SchemeObject car, cdr;
}
+ (id) newWithCar: (SchemeObject) a cdr: (SchemeObject) d;
- (id) initWithCar: (SchemeObject) a cdr: (SchemeObject) d;
- (SchemeObject) car;
- (void) car: (SchemeObject) a;
- (SchemeObject) cdr;
- (void) cdr: (SchemeObject) d;
@end

@extern Cons cons (SchemeObject car, SchemeObject cdr);
@extern BOOL isList (SchemeObject ls);

#endif //__Cons_h
