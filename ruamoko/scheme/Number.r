#include "Number.h"
#include "legacy_string.h"
#include "string.h"

@implementation Number

+ (id) newFromInt: (int) i
{
    return [[self alloc] initWithInt: i];
}

- (id) initWithInt: (int) i
{
    value = i;
    return [super init];
}

- (int) intValue
{
    return value;
}

- (string) printForm
{
    return itos (value);
}

@end
