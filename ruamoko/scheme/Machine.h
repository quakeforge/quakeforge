#ifndef __Machine_h
#define __Machine_h
#include "Symbol.h"
#include "CompiledCode.h"
#include "Frame.h"
#include "Continuation.h"
#include "hash.h"
#include "state.h"

@interface Machine: SchemeObject
{
    state_t state;
    SchemeObject *value;
    hashtab_t *globals;
    SchemeObject *all_globals;
}
- (void) loadCode: (CompiledCode *) code;
- (SchemeObject *) run;
- (void) addGlobal: (Symbol *) sym value: (SchemeObject *) val;
- (void) environment: (Frame *) e;
- (void) continuation: (Continuation *) c;
- (Continuation *) continuation;
- (void) value: (SchemeObject *) o;
- (SchemeObject *) stack;
- (void) stack: (SchemeObject *) o;
- (state_t *) state;
- (void) state: (state_t *) st;
- (void) reset;
- (void) procedure: (Procedure *) pr;
@end

#endif //__Machine_h
