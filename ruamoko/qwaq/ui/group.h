#ifndef __qwaq_ui_group_h
#define __qwaq_ui_group_h

#include <Array.h>

#include "ruamoko/qwaq/ui/event.h"
#include "ruamoko/qwaq/ui/draw.h"

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
+(Group *)withContext:(id<TextContext>)context owner:(View *)owner;
-initWithContext:(id<TextContext>)context owner:(View *)owner;
-(id<TextContext>)context;
-setContext: (id<TextContext>) context;
-insert: (View *) view;
-insertDrawn: (View *) view;
-insertSelected: (View *) view;
-remove: (View *) view;
-(Rect) rect;
-(Rect) absRect;
-(Point) origin;
-(Extent) size;
-draw;
-redraw;
-updateAbsPos: (Point) absPos;
-resize: (Extent) delta;
-handleEvent: (qwaq_event_t *) event;
-takeFocus;
-loseFocus;
-selectNext;
-selectPrev;
-selectView: (View *) view;
-(void) grabMouse;
-(void) releaseMouse;
@end

#endif//__qwaq_ui_group_h
