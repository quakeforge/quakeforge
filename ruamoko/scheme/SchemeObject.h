#ifndef __SchemeObject_h
#define __SchemeObject_h
#include "Object.h"
#define true YES
#define false NO

@interface SchemeObject: Object
{
	BOOL marked;
}
- (void) mark;
- (BOOL) sweep;
//+ (id) alloc;
- (string) printForm;
@end

#endif //__SchemeObject_h
