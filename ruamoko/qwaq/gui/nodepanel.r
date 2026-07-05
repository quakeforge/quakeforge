#include "gui/virtinput.h"

@interface NodePanel : Window
{
	Array *buttons;
	Array *axes;

	ListView *virtView;
}
+panel:(imui_ctx_t)ctx buttons:(Array*)buttons axes:(Array*)axes;
@end

@implementation NodePanel
-initWithContext:(imui_ctx_t)ctx buttons:(Array*)buttons axes:(Array*)axes
{
	if (!(self = [super initWithContext:ctx name:"NodePanel"])) {
		return nil;
	}
	self.buttons = [buttons retain];
	self.axes = [axes retain];
	//IMUI_Window_SetAutoFit (window, false);
	int         width = Draw_Width ();
	int         height = Draw_Height ();
	IMUI_Window_SetPos (window, {50, 50});
	IMUI_Window_SetSize (window, {width-100, height-100});

	virtView = [[ListView list:"NodePanel:virt" ctx:ctx] retain];
	[virtView setItems:buttons];
	return self;
}

-(void)dealloc
{
	[buttons release];
	[axes release];
	[super dealloc];
}

+panel:(imui_ctx_t)ctx buttons:(Array*)buttons axes:(Array*)axes
{
	return [[[self alloc] initWithContext:ctx buttons:buttons axes:axes] autorelease];
}

-draw
{
	if (![super draw]) {
		return nil;
	}
	int         width = Draw_Width ();
	int         height = Draw_Height ();
	UI_Panel(window) {
		UI_Horizontal {
			IMUI_Layout_SetYSize (IMUI_context, imui_size_expand, 100);
			UI_SetFill (current_style.background.normal);
			UI_Vertical {
				IMUI_Layout_SetXSize (IMUI_context, imui_size_expand, 100);
				UI_Label ("hi there");
			}
			[virtView draw];
		};

		uint dent = IMUI_ActiveItem (IMUI_context,
									 imui_size_pixels, 25,
									 imui_size_pixels, 25,
									 sprintf ("source_%p", self));
		IMUI_SetDropTarget (IMUI_context, true);
		IMUI_SetViewPos (IMUI_context, {50, 50});
		IMUI_SetViewFree (IMUI_context, {true, true});
		IMUI_SetViewGravity (IMUI_context, grav_northwest);

		int mode = IMUI_UpdateHotActive (IMUI_context);
		IMUI_CheckButtonState (IMUI_context);
		UI_SetFill (current_style.foreground.color[mode]);

		uint tent = IMUI_ActiveItem (IMUI_context,
									 imui_size_pixels, 25,
									 imui_size_pixels, 25,
									 sprintf ("target_%p", self));
		IMUI_SetDropTarget (IMUI_context, true);
		IMUI_SetViewPos (IMUI_context, {50, 50});
		IMUI_SetViewFree (IMUI_context, {true, true});
		IMUI_SetViewGravity (IMUI_context, grav_southeast);

		mode = IMUI_UpdateHotActive (IMUI_context);
		IMUI_CheckButtonState (IMUI_context);
		UI_SetFill (current_style.foreground.color[mode]);

		auto io = IMUI_GetIO (IMUI_context);
		vec2 start = vec2(io.mouse - io.mouse_active) + 13;
		vec2 end = vec2(io.mouse);
		if (io.active == dent || io.active == tent) {
			IMUI_SetDragId (IMUI_context, io.active);
		}
		io = IMUI_GetIO (IMUI_context);
		if (!io.pressed && io.drag_id != ~0u) {
			Painter_AddBezier (start, {(start.x + end.x)/2, start.y},
							   {(start.x + end.x)/2, end.y}, end,
							   3, {0.85, 0.5, 0.77, 1});
		}
	}
	return self;
}
@end

void node_panel_test ()
{
	arp_end ();
	arp_start ();
	auto buttons = IN_ListButtons ();
	auto button_set = [Array array];
	for (auto b = buttons; *b; b++) {
		[button_set addObject:[VirtualInput button:*b ctx:imui_ctx]];
	}
	auto axes = IN_ListAxes ();
	auto axis_set = [Array array];
	for (auto a = axes; *a; a++) {
		[axis_set addObject:[VirtualInput axis:*a ctx:imui_ctx]];
	}
	auto node_panel = [[NodePanel panel:imui_ctx buttons:button_set axes:axis_set] retain];
}
