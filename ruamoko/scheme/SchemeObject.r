#include "SchemeObject.h"

@implementation SchemeObject

/*
+ (id) alloc
{
    return [super alloc];
}
*/
- (void) mark
{
    marked = true;
}

- (BOOL) sweep
{
    if (marked) {
            marked = false;
            return false;
    } else {
            [self release];
            return true;
    }
}

- (string) printForm
{
    return "<generic>";
}

@end
