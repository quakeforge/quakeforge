#ifndef __ruamoko_gui_View_h
#define __ruamoko_gui_View_h

#include "Object.h"
#include "gui/Rect.h"

/**	\defgroup gui GUI goo for gooey chewing
*/

/**	\addtogroup gui */
///@{

/** The View class.
*/
@interface View: Object
{
@public
	int xpos, ypos;
	int xlen, ylen;
	int xabs, yabs;
	View	*parent;
	int flags;
}

- (id) initWithComponents: (int)x : (int)y : (int)w : (int)h;
- (id) initWithOrigin: (Point)anOrigin size: (Size)aSize;
- (id) initWithBounds: (Rect)aRect;
- (id) canFocus: (int)cf;
- (int) canFocus;
- (void) setBasePos: (int)x y: (int)y;
- (void) setBasePosFromView: (View*)view;
- (void) draw;

- (int) keyEvent:(int)key unicode:(int)unicode down:(int)down;
@end

///@}

#endif //__ruamoko_gui_View_h
