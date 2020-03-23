#ifndef __qwaq_group_h
#define __qwaq_group_h

#include <Array.h>

#include "event.h"
#include "qwaq-draw.h"

@class View;

@interface Group : Object
{
	View       *owner;
	Array      *views;
	View       *mouse_grabbed;
	View       *mouse_within;
	int         focused;
	id<TextContext> context;
}
-initWithContext: (id<TextContext>) context owner: (View *) owner;
-insert: (View *) view;
-remove: (View *) view;
-(Rect) rect;
-(Point) origin;
-(Extent) size;
-draw;
-redraw;
-resize: (Extent) delta;
-handleEvent: (qwaq_event_t *) event;
-(void) grabMouse;
-(void) releaseMouse;
@end

#endif//__qwaq_group_h
