#include "string.h"
#include "String.h"

@implementation String

+ (id) newFromString: (string) s
{
    return [[self alloc] initWithString: s];
}

- (id) initWithString: (string) s
{
    self = [super init];
    value = str_new();
    str_copy(value, s);
    return self;
}

- (string) stringValue
{
    return value;
}

- (string) printForm
{
    return value;
}

- (void) dealloc
{
    str_free (value);
    [super dealloc];
}

@end
