#ifndef __Frame_h
#define __Frame_h
#include "SchemeObject.h"

@interface Frame: SchemeObject
{
    SchemeObject**array;
    int size;
    Frame *link;
}
+ (id) newWithSize: (int) sz link: (Frame *) l;
- (id) initWithSize: (int) sz link: (Frame *) l;
- (void) set: (int) index to: (SchemeObject *) o;
- (SchemeObject *) get: (int) index;
- (Frame *) getLink;
@end

#endif //__FOO_h
