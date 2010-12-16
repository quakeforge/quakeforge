#include "BaseContinuation.h"
#include "Cons.h"

instruction_t returninst;
BaseContinuation base;

@implementation BaseContinuation
+ (void) initialize
{
    returninst.opcode = RETURN;
    base = [BaseContinuation new];
}
+ (id) baseContinuation
{
    return base;
}

- (void) restoreOnMachine: (Machine) m
{
    [m state].program = &returninst;
}

- (void) invokeOnMachine: (Machine) m
{
    [m value: [(Cons) [m stack] car]];
    [m state].program = &returninst;
}

@end
