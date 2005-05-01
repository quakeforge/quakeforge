#include "Void.h"

Void voidConstant;

@implementation Void

+ (void) initialize
{
    voidConstant = [Void new];
}

+ (id) voidConstant
{
    return voidConstant;
}

- (string) printForm
{
    return "<void>";
}

@end
