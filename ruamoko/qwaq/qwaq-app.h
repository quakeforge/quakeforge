#ifndef __qwaq_app_h
#define __qwaq_app_h

#include <Object.h>

#include "event.h"

@class Group;
@class TextContext;

@interface QwaqApplication: Object
{
	qwaq_event_t event;
	qwaq_command endState;

	Group      *objects;

	TextContext *screen;
	int         autocount;
}
-run;
-draw;
-handleEvent: (qwaq_event_t *) event;
@end

#endif//__qwaq_app_h
