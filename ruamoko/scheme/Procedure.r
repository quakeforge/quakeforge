#include "Procedure.h"
#include "Machine.h"

@implementation Procedure
- (void) invokeOnMachine: (Machine) m
{
    [m procedure: self];
    return;
}

- (string) printForm
{
    return "<procedure>";
}

@end
