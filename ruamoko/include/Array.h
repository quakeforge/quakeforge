#ifndef __ruamoko_Array_h
#define __ruamoko_Array_h

#include "Object.h"

@interface Array: Object
{
	integer count, size;
	integer incr;
	id [] array;
}
- (id) init;
- (id) initWithIncrement: (integer) inc;
- (void) free;
- (id) getItemAt: (integer) index;
- (void) setItemAt: (integer) index item:(id) item;
- (void) addItem: (id) item;
- (void) removeItem: (id) item;
- (id) removeItemAt: (integer) index;
- (id) insertItemAt: (integer) index item:(id) item;
- (integer) count;
@end

#endif//__ruamoko_Array_h
