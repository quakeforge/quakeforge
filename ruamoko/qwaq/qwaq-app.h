#ifndef __qwaq_app_h
#define __qwaq_app_h

#include "qwaq-view.h"

#include "event.h"

@interface QwaqApplication: Object
{
	qwaq_event_t event;
	qwaq_command endState;
	View *view;
}
-run;
-handleEvent: (qwaq_event_t *) event;
@end

#endif//__qwaq_app_h
