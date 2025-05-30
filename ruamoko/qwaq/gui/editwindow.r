#include <Array.h>

#include <stdlib.h>
#include <dirent.h>
#include <imui.h>
#include <string.h>
#include "editwindow.h"

void printf(string, ...);

@implementation EditWindow

-initWithFile:(string)filePath ctx:(imui_ctx_t)ctx
{
	if (!(self = [super init])) {
		return nil;
	}
	IMUI_context = ctx;
	window_name = str_hold (filePath);
	window = IMUI_NewWindow (window_name);

	string evname = sprintf ("EditView:%0x8", window);
	editView = [[EditView edit:evname file:filePath ctx:ctx] retain];

	IMUI_Window_SetSize (window, {400, 300});
	return self;
}

+(EditWindow *) openFile:(string)filePath ctx:(imui_ctx_t)ctx
{
	return [[[self alloc] initWithFile:filePath ctx:ctx]
			autorelease];
}

-(void)dealloc
{
	[editView release];
	IMUI_DeleteWindow (window);
	str_free (window_name);
	[super dealloc];
}

-draw
{
	imui_style_t style = {};//FIXME qfcc bug
	IMUI_Style_Fetch (IMUI_context, &style);
	UI_Window (window) {
		if (IMUI_Window_IsCollapsed (window)) {
			continue;
		}
		UI_Horizontal {
			IMUI_Layout_SetYSize (IMUI_context, imui_size_expand, 100);
			UI_SetFill (style.background.normal);
			[editView draw];
		}
	}
	return self;
}

@end
