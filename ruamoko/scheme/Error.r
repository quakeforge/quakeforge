#include "Error.h"
#include "string.h"

@implementation Error
+ (id) type: (string) t message: (string) m by: (SchemeObject *) o
{
    return [[self alloc] initWithType: t message: m by: o];
}

+ (id) type: (string) t message: (string) m
{
    return [[self alloc] initWithType: t message: m by: nil];
}

- (id) initWithType: (string) t message: (string) m by: (SchemeObject *) o
{
    self = [super init];
    type = str_new();
    message = str_new();
    str_copy(type, t);
    str_copy(message, m);
    if (o) {
            [self source: [o source]];
            [self line: [o line]];
    }
    return self;
}

- (BOOL) isError
{
    return true;
}

- (string) type
{
    return type;
}

- (string) message
{
    return message;
}

- (void) dealloc
{
    str_free(type);
    str_free(message);
    [super dealloc];
}
@end
