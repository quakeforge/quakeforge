#ifndef __qwaq_gui_ui_object_h
#define __qwaq_gui_ui_object_h

#include <Object.h>

typedef @handle imui_ctx_h imui_ctx_t;

@interface UI_Object : Object
{
	imui_ctx_t IMUI_context;
}
-initWithContext:(imui_ctx_t)ctx;
@end

#endif//__qwaq_gui_ui_object_h
