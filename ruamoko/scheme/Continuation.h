#ifndef __Continuation_h
#define __Continuation_h
#include "SchemeObject.h"
#include "Procedure.h"
#include "state.h"

@interface Continuation: Procedure
{
    state_t state;
    SchemeObject cont;
    Frame env;
}
+ (id) newWithState: (state_t []) st environment: (Frame) e continuation: (Continuation) c pc: (integer) p;
- (id) initWithState: (state_t []) st environment: (Frame) e continuation: (Continuation) c pc: (integer) p;

@end

#endif //__Procedure_h
