#ifndef __qwaq_ui_button_h
#define __qwaq_ui_button_h

#include "ruamoko/qwaq/ui/draw.h"
#include "ruamoko/qwaq/ui/view.h"

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
+(Button *)withPos: (Point) pos releasedIcon: (DrawBuffer *) released
								 pressedIcon: (DrawBuffer *) pressed;
+(Button *)withRect: (Rect) rect;	// invisible button
-initWithPos: (Point) pos releasedIcon: (DrawBuffer *) released
						   pressedIcon: (DrawBuffer *) pressed;
-initWithRect: (Rect) rect;	// invisible button
-(ListenerGroup *) onPress;
-(ListenerGroup *) onRelease;
-(ListenerGroup *) onClick;
-(ListenerGroup *) onDrag;
-(ListenerGroup *) onAuto;
-(ListenerGroup *) onHover;

- (int) click;
- (Point) delta;
@end

#endif//__qwaq_ui_button_h
