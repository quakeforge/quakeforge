#ifndef __ruamoko_gui_Slider_h
#define __ruamoko_gui_Slider_h

#include "View.h"

@interface Slider: View
{
	integer index;
	integer size;
	integer dir;
}

- (id) initWithBounds: (Rect)aRect size: (integer) aSize;
- (void) setIndex: (integer) ind;
- (void) draw;

@end

#endif //__ruamoko_gui_Slider_h
