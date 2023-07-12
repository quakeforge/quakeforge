#ifndef __ruamoko_gui_Slider_h
#define __ruamoko_gui_Slider_h

#include <gui/View.h>

/**	\addtogroup gui */
///@{

@interface Slider: View
{
	int index;
	int size;
	int dir;
}

- (id) initWithBounds: (Rect)aRect size: (int) aSize;
- (void) setIndex: (int) ind;
- (void) draw;

@end

///@}

#endif //__ruamoko_gui_Slider_h
