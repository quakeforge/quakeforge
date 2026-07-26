#include <imui.h>
#include <string.h>

#include "slider.h"

void printf(string fmt, ...);

@implementation Slider

-init:(vec2)range step:(float)step ctx:(imui_ctx_t)ctx
{
	if (!(self = [super initWithContext:ctx])) {
		return nil;
	}
	self.range = range;
	self.step = step;
	return self;
}

+(Slider *)slider:(vec2)range step:(float)step ctx:(imui_ctx_t)ctx
{
	return [[[self alloc] init:range step:step ctx:ctx] autorelease];
}

-(bool) draw:(@inout float)val
{
	imui_style_t style;
	IMUI_Style_Fetch (IMUI_context, &style);
	UI_SetFill (style.background.normal);
	uint dent = IMUI_ActiveItem (IMUI_context,
								 imui_size_percent, 100,
								 imui_size_pixels, 25,
								 sprintf ("slider_%p", self));
	IMUI_SetViewPos (IMUI_context, {0, 0});
	IMUI_SetViewFree (IMUI_context, {true, true});
	IMUI_SetViewGravity (IMUI_context, grav_northwest);

	int mode = IMUI_UpdateHotActive (IMUI_context);
	IMUI_CheckButtonState (IMUI_context);
	UI_SetFill (style.foreground.color[mode]);

	auto io = IMUI_GetIO (IMUI_context);
	if (io.active == dent) {
		IMUI_SetDragId (IMUI_context, io.active);
	}
	io = IMUI_GetIO (IMUI_context);
	if (io.drag_id != dent) {
		return false;
	}
	if (io.pressed == 1) {
		drag_start = io.mouse_active;
		start_val = val;
	}
	float delta = (io.mouse_active.x - drag_start.x) * step;
	val = start_val + delta;
	if (val > range.y) {
		val = range.y;
	}
	if (val < range.x) {
		val = range.x;
	}
	return true;
}
@end
