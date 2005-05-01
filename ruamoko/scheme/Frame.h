#ifndef __Frame_h
#define __Frame_h
#include "SchemeObject.h"

@interface Frame: SchemeObject
{
    SchemeObject[] array;
    integer size;
    Frame link;
}
+ (id) newWithSize: (integer) sz link: (Frame) l;
- (id) initWithSize: (integer) sz link: (Frame) l;
- (void) set: (integer) index to: (SchemeObject) o;
- (SchemeObject) get: (integer) index;
- (Frame) getLink;
@end

#endif //__FOO_h
