#ifndef __qwaq_gui_window_h
#define __qwaq_gui_window_h

#include <Object.h>

#include "ui_object.h"

typedef struct imui_window_s imui_window_t;

@interface Window : UI_Object
{
	imui_window_t *window;
}
+(void)shutdown;
+(void)drawWindows;
+(void)drawMenuItems;

-initWithContext:(imui_ctx_t)ctx name:(string)name;
-(string)name;
-draw;
-raise;
-close;
@end

#endif//__qwaq_gui_window_h
