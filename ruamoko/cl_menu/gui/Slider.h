#ifndef __ruamoko_cl_menu_gui_Slider_h
#define __ruamoko_cl_menu_gui_Slider_h

#include "View.h"

/**	\addtogroup gui */
///@{

@interface Slider: View
{
	int index;
	int size;
	bool dir;
}

- (id) initWithBounds: (Rect)aRect size: (int) aSize;
- (void) setIndex: (int) ind;
- (void) draw;

@end

///@}

#endif //__ruamoko_cl_menu_gui_Slider_h
