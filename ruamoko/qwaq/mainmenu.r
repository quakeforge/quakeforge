#include <imui.h>
#include <string.h>

#include "gui/editwindow.h"

#include "mainmenu.h"

@implementation MainMenu
+(imui_window_t *)create_menu:(string)name
{
	auto menu = IMUI_NewWindow (name);
	IMUI_Window_SetOpen (menu, false);
	IMUI_Window_SetGroupOffset (menu, 1);
	IMUI_Window_SetReferenceGravity (menu, grav_northwest);
	IMUI_Window_SetAnchorGravity (menu, grav_southwest);
	IMUI_Window_SetAutoFit (menu, true);
	return menu;
}

static void
hs (imui_ctx_t ctx)
{
	IMUI_Spacer (ctx, imui_size_pixels, 10, imui_size_expand, 100);
}

-initWithContext:(imui_ctx_t)ctx
{
	if (!(self = [super initWithContext:ctx])) {
		return nil;
	}
	main_menu = IMUI_NewWindow ("MainMenu");
	IMUI_Window_SetOpen (main_menu, true);
	IMUI_Window_SetNoCollapse (main_menu, true);
	IMUI_Window_SetAutoFit (main_menu, true);

	file_menu = [MainMenu create_menu:"File"];
	window_menu = [MainMenu create_menu:"Window"];

	file_window = nil;

	return self;
}

+(MainMenu *) menu:(imui_ctx_t)ctx
{
	return [[[MainMenu alloc] initWithContext:ctx] autorelease];
}

-draw
{
	UI_MenuBar (main_menu) {
		UI_Menu (file_menu) {
			if (UI_MenuItem (sprintf ("Open##%p", file_menu))) {
				file_window = [FileWindow openFile:"*.r" at:"."
									ctx:IMUI_context];
				[file_window setTarget:self];
			}
			if (UI_MenuItem (sprintf ("Quit##%p", file_menu))) {
				quit_editor = true;
			}
		}
		hs (IMUI_context);
		UI_Menu (window_menu) {
			[Window drawMenuItems];
		}
	}
	return self;
}

-(void)openFile:(string)path forSave:(bool)forSave
{
	if (forSave) {
	} else {
		[EditWindow openFile:path ctx:IMUI_context];
		[file_window close];
		file_window = nil;
	}
}
@end
