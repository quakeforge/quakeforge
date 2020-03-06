#ifndef __qwaq_screen_h
#define __qwaq_screen_h

#include "qwaq-draw.h"
#include "qwaq-rect.h"
#include "qwaq-view.h"

@interface Screen: View
{
}
+(Screen *) screen;
-handleEvent: (qwaq_event_t *) event;
-setBackground: (int) ch;
@end

#endif//__qwaq_screen_h
