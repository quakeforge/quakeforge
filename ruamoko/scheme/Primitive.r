#include "Primitive.h"
#include "Machine.h"
#include "Continuation.h"

@implementation Primitive
+ (id) newFromFunc: (primfunc_t) f
{
    return [[self alloc] initWithFunc: f];
}
- (id) initWithFunc: (primfunc_t) f
{
    self = [super init];
    func = f;
    return self;
}
- (SchemeObject) invokeOnMachine: (Machine) m
{
    local SchemeObject value = func ([m stack], m);
    [super invokeOnMachine: m];
    if (value) {
            [m value: value];
            [[m continuation] restoreOnMachine: m];
    }
}

- (string) printForm
{
    return "<primitive>";
}

@end
