#include <Array.h>
#include <imui.h>
#include <string.h>

#include "window.h"

@implementation Window

static Array *windows;

+(void) initialize
{
	windows = [[Array array] retain];
}

+(void)shutdown
{
	[windows release];
}
-initWithContext:(imui_ctx_t)ctx name:(string)name
{
	if (!(self = [super initWithContext:ctx])) {
		return nil;
	}
	window = IMUI_NewWindow (name);
	[windows addObject:self];
	return self;
}

-(string)name
{
	return IMUI_Window_GetName (window);
}

+(void)drawWindows
{
	[windows makeObjectsPerformSelector: @selector (draw)];
}

+(void)drawMenuItems
{
	uint count = [windows count];
	if (!count) {
		return;
	}
	auto IMUI_context = ((Window *)[windows objectAtIndex:0]).IMUI_context;
	for (uint i = 0; i < count; i++) {
		id w = [windows objectAtIndex:i];
		string name = [w name];
		if (UI_MenuItem (sprintf ("%s##%p:%u", name, self, i))) {
			[w raise];
		}
	}
}

-draw
{
	if (!IMUI_Window_IsOpen (window)) {
		[windows removeObject:self];
		return nil;
	}
	return self;
}

-raise
{
	IMUI_RaiseWindow (IMUI_context, window);
	return self;
}

-close
{
	IMUI_Window_SetOpen (window, false);
	return self;
}

@end
