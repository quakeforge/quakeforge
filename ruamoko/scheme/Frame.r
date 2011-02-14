#include "Frame.h"

@implementation Frame
+ (id) newWithSize: (integer) sz link: (Frame *) l
{
    return [[self alloc] initWithSize: sz link: l];
}

- (id) initWithSize: (integer) sz link: (Frame *) l
{
    self = [super init];
    size = sz;
    link = l;
    array = obj_malloc(@sizeof (id) * size);
    return self;
}

- (void) set: (integer) index to: (SchemeObject *) o
{
    array[index] = o;
}

- (SchemeObject *) get: (integer) index
{
    return array[index];
}

- (Frame *) getLink
{
    return link;
}

- (void) markReachable
{
    local integer index;
    for (index = 0; index < size; index++) {
            [array[index] mark];
    }
    [link mark];
}

- (void) dealloc
{
    obj_free(array);
    [super dealloc];
}

@end
