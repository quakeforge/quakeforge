#ifndef __qwaq_gui_slider_h
#define __qwaq_gui_slider_h

#include "ui_object.h"

@interface Slider : UI_Object
{
	float start_val;
	vec2 range;
	float step;
	ivec2 drag_start;
}
+(Slider *)slider:(vec2)range step:(float)step ctx:(imui_ctx_t)ctx;
-(bool)draw:(@inout float)val;
@end

#endif//__qwaq_gui_slider_h
