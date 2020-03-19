#ifndef __qwaq_button_h
#define __qwaq_button_h

#include "qwaq-draw.h"
#include "qwaq-view.h"

@class ListenerGroup;

@interface Button : View
{
	DrawBuffer *icon[2];
	int         pressed;
	ListenerGroup *onPress;
	ListenerGroup *onRelease;
	ListenerGroup *onClick;
	ListenerGroup *onDrag;
	ListenerGroup *onAuto;
}
-initWithPos: (Point) pos releasedIcon: (DrawBuffer *) released
						   pressedIcon: (DrawBuffer *) pressed;
-(ListenerGroup *) onPress;
-(ListenerGroup *) onRelease;
-(ListenerGroup *) onClick;
-(ListenerGroup *) onDrag;
-(ListenerGroup *) onAuto;
@end

#endif//__qwaq_button_h
