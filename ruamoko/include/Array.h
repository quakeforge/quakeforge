#ifndef __ruamoko_Array_h
#define __ruamoko_Array_h

#include "Object.h"

@interface Array: Object
{
	integer count, size;
	integer incr;
	(void [])[]array;
}
- (id) init;
- (id) initWithIncrement: (integer) inc;
- (void) free;
- (void []) getItemAt: (integer) index;
- (void) setItemAt: (integer) index item:(void []) item;
- (void) addItem: (void []) item;
- (void []) removeItemAt: (integer) index;
- (void []) insertItemAt: (integer) index item:(void []) item;
@end

#endif//__ruamoko_Array_h
