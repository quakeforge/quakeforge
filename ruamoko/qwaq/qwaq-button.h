#ifndef __qwaq_button_h
#define __qwaq_button_h

#include "qwaq-draw.h"
#include "qwaq-view.h"

@class ListenerGroup;

@interface Button : View
{
	DrawBuffer *icon[2];
	int         pressed;
	int         click;
	Point       dragBase;
	Point       dragPos;
	ListenerGroup *onPress;
	ListenerGroup *onRelease;
	ListenerGroup *onClick;
	ListenerGroup *onDrag;
	ListenerGroup *onAuto;
	ListenerGroup *onHover;
}
-initWithPos: (Point) pos releasedIcon: (DrawBuffer *) released
						   pressedIcon: (DrawBuffer *) pressed;
-(ListenerGroup *) onPress;
-(ListenerGroup *) onRelease;
-(ListenerGroup *) onClick;
-(ListenerGroup *) onDrag;
-(ListenerGroup *) onAuto;
-(ListenerGroup *) onHover;

- (int) click;
- (Point) delta;
@end

#endif//__qwaq_button_h
