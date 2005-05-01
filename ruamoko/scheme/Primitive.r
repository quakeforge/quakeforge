#include "Primitive.h"
#include "Machine.h"

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
    [m value: value];
    [[m continuation] invokeOnMachine: m];
}

- (string) printForm
{
    return "<primitive>";
}

@end
