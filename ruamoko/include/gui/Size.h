#ifndef __ruamoko_gui_Size_h
#define __ruamoko_gui_Size_h

#include "Object.h"

@interface Size: Object
{
@public
	integer	width;
	integer	height;
}

- (id) initWithComponents: (integer)w : (integer)h;
- (id) initWithSize: (Size)aSize;
- (id) copy;

- (integer) width;
- (integer) height;

- (void) setSize: (Size)aSize;
- (void) setWidth: (integer)w;
- (void) setHeight: (integer)h;

- (void) addSize: (Size)aSize;
- (void) subtractSize: (Size)aSize;

@end

#endif //__ruamoko_gui_Size_h
