#include "Frame.h"

@implementation Frame
+ (id) newWithSize: (int) sz link: (Frame *) l
{
    return [[self alloc] initWithSize: sz link: l];
}

- (id) initWithSize: (int) sz link: (Frame *) l
{
    self = [super init];
    size = sz;
    link = l;
    array = obj_malloc(@sizeof (id) * size);
    return self;
}

- (void) set: (int) index to: (SchemeObject *) o
{
    array[index] = o;
}

- (SchemeObject *) get: (int) index
{
    return array[index];
}

- (Frame *) getLink
{
    return link;
}

- (void) markReachable
{
    local int index;
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
