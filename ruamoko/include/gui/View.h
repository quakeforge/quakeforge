#ifndef __ruamoko_View_h
#define __ruamoko_View_h

#include "Object.h"

@class Point;
@class Size;
@class Rect;

@interface View: Object
{
@public
	integer xpos, ypos;
	integer xlen, ylen;
	integer xabs, yabs;
	View	parent;
}

- (id) initWithComponents: (integer)x : (integer)y : (integer)w : (integer)h;
- (id) initWithOrigin: (Point)anOrigin size: (Size)aSize;
- (id) initWithBounds: (Rect)aRect;
- (void) setBasePos: (integer) x y: (integer) y;
- (void) draw;
@end

#endif //__ruamoko_View_h
