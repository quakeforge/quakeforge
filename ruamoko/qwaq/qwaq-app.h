#ifndef __qwaq_app_h
#define __qwaq_app_h

#include <Object.h>

#include "event.h"

@class Group;

@interface QwaqApplication: Object
{
	qwaq_event_t event;
	qwaq_command endState;

	Group      *objects;
}
-run;
-draw;
-handleEvent: (qwaq_event_t *) event;
@end

#endif//__qwaq_app_h
