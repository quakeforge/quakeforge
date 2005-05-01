#include "Number.h"
#include "string.h"

@implementation Number

+ (id) newFromInt: (integer) i
{
    return [[self alloc] initWithInt: i];
}

- (id) initWithInt: (integer) i
{
    value = i;
    return [super init];
}

- (integer) intValue
{
    return value;
}

- (string) printForm
{
    return itos (value);
}

@end
