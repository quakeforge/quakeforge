#include "Lambda.h"
#include "Nil.h"
#include "Symbol.h"
#include "string.h"
#include "Cons.h"
#include "defs.h"
#include "Error.h"
#include "Machine.h"

@implementation Lambda
+ (id) newWithCode: (CompiledCode) c environment: (Frame) e
{
    return [[self alloc] initWithCode: c environment: e];
}
            
- (id) initWithCode: (CompiledCode) c environment: (Frame) e
{
    self = [super init];
    code = c;
    env = e;
    return self;
}

- (void) invokeOnMachine: (Machine) m
{
    [super invokeOnMachine: m];
    if (length([m stack]) < [code minimumArguments]) {
            [m value: [Error type: "call"
                             message: sprintf("expected at least %i arguments, received %i",
                                              [code minimumArguments], length([m stack]))
                             by: m]];
            return;
    }
    [m loadCode: code];
    [m environment: env];
    [m procedure: self];
}

- (void) markReachable
{
    [env mark];
    [code mark];
}

@end
