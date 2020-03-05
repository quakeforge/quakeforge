#ifndef __qwaq_app_h
#define __qwaq_app_h

#include "event.h"
#include "qwaq-group.h"

@class Screen;

@interface QwaqApplication: Group
{
	qwaq_event_t event;
	qwaq_command endState;
	Screen     *screen;
}
-run;
-handleEvent: (qwaq_event_t *) event;
@end

#endif//__qwaq_app_h
