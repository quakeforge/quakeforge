#ifndef __ruamoko_gui_Group_h
#define __ruamoko_gui_Group_h

#include "View.h"

@class Array;

@interface Group : View
{
	Array views;
}
- (id) initWithBounds: (Rect)bounds;
- (void) dealloc;
- (View) addView: (View)aView;
- (void) moveTo: (integer)x y:(integer)y;
- (void) setBasePos: (Point)pos;
- (void) draw;
@end

#endif//__ruamoko_gui_Group_h
