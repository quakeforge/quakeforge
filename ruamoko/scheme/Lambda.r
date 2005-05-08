#include "Lambda.h"
#include "Nil.h"
#include "Symbol.h"
#include "string.h"
#include "defs.h"

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
