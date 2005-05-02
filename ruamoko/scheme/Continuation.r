#include "Continuation.h"
#include "defs.h"

@implementation Continuation
+ (id) newWithState: (state_t []) st environment: (Frame) e continuation: (Continuation) c pc: (integer) p
{
    return [[self alloc] initWithState: st environment: e continuation: c pc: p];
}
- (id) initWithState: (state_t []) st environment: (Frame) e continuation: (Continuation) c pc: (integer) p
{
    self = [self init];
    state.program = st.program;
    state.pc = p;
    state.literals = st.literals;
    state.stack = st.stack;
    cont = c;
    env = e;
    return self;
}

- (void) invokeOnMachine: (Machine) m
{
    [m state: &state];
    [m environment: env];
    [m continuation: cont];
    return;
}

- (void) markReachable
{
    [state.literals mark];
    [state.stack mark];
    [cont mark];
    [env mark];
}

- (string) printForm
{
    return "<continuation>";
}

@end
