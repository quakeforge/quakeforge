#ifndef __ruamoko_gui_View_h
#define __ruamoko_gui_View_h

#include "Object.h"
#include "gui/Rect.h"

@interface View: Object
{
@public
	integer xpos, ypos;
	integer xlen, ylen;
	integer xabs, yabs;
	View	[]parent;
	integer flags;
}

- (id) initWithComponents: (integer)x : (integer)y : (integer)w : (integer)h;
- (id) initWithOrigin: (Point)anOrigin size: (Size)aSize;
- (id) initWithBounds: (Rect)aRect;
- (id) canFocus: (integer)cf;
- (integer) canFocus;
- (void) setBasePos: (integer)x y: (integer)y;
- (void) setBasePosFromView: (View[])view;
- (void) draw;

- (integer) keyEvent:(integer)key unicode:(integer)unicode down:(integer)down;
@end

#endif //__ruamoko_gui_View_h
