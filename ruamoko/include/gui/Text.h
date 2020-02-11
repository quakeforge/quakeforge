#ifndef __ruamoko_gui_Text_h
#define __ruamoko_gui_Text_h

#include "View.h"

/**	\addtogroup gui */
///@{

@interface Text: View
{
@public
	string text;
}

- (id) initWithBounds: (Rect)aRect;
- (id) initWithBounds: (Rect)aRect text:(string)str;
- (void) setText: (string)str;
- (void) draw;
@end

///@}

#endif //__ruamoko_gui_Text_h
