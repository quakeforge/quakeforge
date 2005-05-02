#ifndef __SchemeObject_h
#define __SchemeObject_h
#include "Object.h"
#define true YES
#define false NO

//#define DEBUG
#ifdef DEBUG
#define dprintf printf
#else
#define dprintf(x, ...)
#endif


@interface SchemeObject: Object
{
    @public SchemeObject prev, next;
    BOOL marked, root;
}
- (void) mark;
- (void) markReachable;
- (void) makeRootCell;
- (string) printForm;
@end

#endif //__SchemeObject_h
