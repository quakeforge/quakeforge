#ifndef __qwaq_group_h
#define __qwaq_group_h

#include <Array.h>

#include "event.h"
#include "qwaq-draw.h"

@class View;

@interface Group : Object
{
	Array      *views;
	int         focused;
	id<TextContext> context;
}
-initWithContext: (id<TextContext>) context;
-insert: (View *) view;
-remove: (View *) view;
-draw;
-redraw;
-handleEvent: (qwaq_event_t *) event;
@end

#endif//__qwaq_group_h
