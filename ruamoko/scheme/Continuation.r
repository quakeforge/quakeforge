#include "Continuation.h"
#include "Machine.h"
#include "Cons.h"
#include "defs.h"

@implementation Continuation
+ (id) newWithState: (state_t *) st pc: (integer) p
{
    return [[self alloc] initWithState: st pc: p];
}
- (id) initWithState: (state_t *) st pc: (integer) p
{
    self = [self init];
    state.program = st.program;
    state.pc = p;
    state.literals = st.literals;
    state.stack = st.stack;
    state.cont = st.cont;
    state.env = st.env;
    state.proc = st.proc;
    state.lineinfo = st.lineinfo;
    return self;
}

- (void) restoreOnMachine: (Machine *) m
{
    [m state: &state];
    return;
}

- (void) invokeOnMachine: (Machine *) m
{
    [m value: [(Cons *) [m stack] car]];
    [m state: &state];
    return;
}

- (void) markReachable
{
    [state.literals mark];
    [state.stack mark];
    [state.cont mark];
    [state.env mark];
    [state.proc mark];
}

- (string) printForm
{
    return "<continuation>";
}

@end
