#ifndef __ruamoko_View_h
#define __ruamoko_View_h

#include "Object.h"
#include "Point.h"
#include "Size.h"

@interface View: Object
{
@public
	integer xpos, ypos;
	integer xlen, ylen;
	integer xabs, yabs;
	View	parent;
}

- (id) initWithComponents: (integer)x : (integer)y : (integer)w : (integer)h;
- (void) draw;
@end

#endif //__ruamoko_Rect_h
