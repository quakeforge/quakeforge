#include <AutoreleasePool.h>
#include <plist.h>
//#include <PropertyList.h>
#include <string.h>
#include <imui.h>
#include <qfs.h>
#include <draw.h>
#include <scene.h>
#include <QF/keys.h>

#include "debugger/debug.h"

#include "gui/editwindow.h"
#include "gui/filewindow.h"
#include "gui/listview.h"
#include "gui/virtinput.h"
#include "gui/window.h"

void traceon();
void traceoff();
void printf (string fmt, ...) = #0;

static string render_graph_cfg =
#embed "config/qwaq-ed-rg.plist"
;

bool draw_editor_overlay = true;
bool quit_editor = false;

typedef struct component_s component_t;
void init_graphics (plitem_t *config, int num_components,
					component_t *components) = #0;
float refresh (scene_t scene) = #0;
void refresh_2d (void (func)(void)) = #0;
void setpalette (void *palette, void *colormap) = #0;
void newscene (scene_t scene) = #0;
void setevents (int (func)(struct IE_event_s *, void *), void *data) = #0;
void setctxcbuf (int ctx) = #0;
void addcbuftxt (string txt) = #0;

imui_ctx_t imui_ctx;
#define IMUI_context imui_ctx
imui_window_t *style_editor;

imui_style_t current_style = {
	.background = {
		.normal = 0x14,
		.hot = 0x14,
		.active = 0x14,
	},
	.foreground = {
		.normal = 0x18,
		.hot = 0x1f,
		.active = 0x1c,
	},
	.text = {
		.normal = 0xfd,
		.hot = 0xfe,
		.active = 0xea,
	},
};

double realtime = double (1ul<<32);
float frametime;

@class MainMenu;
MainMenu *main_menu;

static void
color_window (void)
{
	if (!style_editor) {
		style_editor = IMUI_NewWindow ("style");
	}
	static int style_selection;
	static int style_mode;
	static int style_color;
	UI_Window (style_editor) {
		if (IMUI_Window_IsCollapsed (style_editor)) {
			continue;
		}
		UI_Vertical {
			UI_Horizontal {
				UI_FlexibleSpace ();
				UI_Radio (&style_selection, 0, "Background");
				UI_FlexibleSpace ();
				UI_Radio (&style_selection, 1, "Foreground");
				UI_FlexibleSpace ();
				UI_Radio (&style_selection, 2, "Text");
				UI_FlexibleSpace ();
			}
			UI_Horizontal {
				UI_FlexibleSpace ();
				UI_Radio (&style_mode, 0, "Normal");
				UI_FlexibleSpace ();
				UI_Radio (&style_mode, 1, "Hot");
				UI_FlexibleSpace ();
				UI_Radio (&style_mode, 2, "Active");
				UI_FlexibleSpace ();
			}
			if (style_selection == 0) {
				style_color = current_style.background.color[style_mode];
			} else if (style_selection == 1) {
				style_color = current_style.foreground.color[style_mode];
			} else if (style_selection == 2) {
				style_color = current_style.text.color[style_mode];
			}
			imui_style_t style;
			IMUI_Style_Fetch (imui_ctx, &style);
			auto bg_save = style.background.normal;
			auto fg_save = style.foreground.normal;
			UI_Style (nil) for (int y = 0; y < 16; y++) {
				UI_Horizontal for (int x = 0; x < 16; x++) {
					if (!x) UI_FlexibleSpace ();
					int c = y * 16 + x;
					int ic = y * 16 + (15-x);
					style.background.normal = c;
					style.foreground.normal = c == style_color ? c : ic;
					IMUI_Style_Update (imui_ctx, &style);
					UI_Radio (&style_color, c, sprintf ("##color_%x%x", y, x));
					if (x == 15) {
						style.background.normal = bg_save;
						style.foreground.normal = fg_save;
						IMUI_Style_Update (imui_ctx, &style);
						UI_FlexibleSpace ();
					}
				}
			}
		}
		if (style_selection == 0) {
			current_style.background.color[style_mode] = style_color;
		} else if (style_selection == 1) {
			current_style.foreground.color[style_mode] = style_color;
		} else if (style_selection == 2) {
			current_style.text.color[style_mode] = style_color;
		}
	}
}

@interface MainMenu : UI_Object<FileWindow>
{
	imui_window_t *main_menu;
	imui_window_t *file_menu;
	imui_window_t *window_menu;
	FileWindow *file_window;
}
+(imui_window_t *)create_menu:(string)name;
+(MainMenu *) menu:(imui_ctx_t)ctx;
-draw;
@end

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
				file_window = [FileWindow openFile:"*.dat" at:"."
									ctx:imui_ctx];
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
		//[EditWindow openFile:path ctx:imui_ctx];
		qdb_load_progs (path);
		[file_window close];
		file_window = nil;
	}
}
@end

static void
draw_2d (void)
{
	int         width = Draw_Width ();
	int         height = Draw_Height ();

	Draw_String (8, height - 8, sprintf ("%5.2f", frametime*1000));

	if (draw_editor_overlay) {
		IMUI_SetSize (imui_ctx, Draw_Width (), Draw_Height ());
		IMUI_BeginFrame (imui_ctx);
		IMUI_Style_Update (imui_ctx, &current_style);

		[main_menu draw];
		[Window drawWindows];
		//color_window ();
		IMUI_Draw (imui_ctx);
	}
}

static int
event_handler (IE_event_t *event, void *data)
{
	switch (event.type) {
	case ie_message:
		printf ("message: %d %d\n", event.message.code, event.message.int_val);
		auto target = (qdb_target_t) event.message.int_val;
		qdb_event_t dbg_event;
		if (qdb_get_event (target, &dbg_event)) {
			printf ("    %d %d\n", dbg_event.what, dbg_event.exit_code);
			if (dbg_event.what == prd_error
				|| dbg_event.what == prd_runerror) {
				printf ("    %s\n", dbg_event.message);
			}
		}
		return event.message.code == 0;
	case ie_mouse:
		return IMUI_ProcessEvent (imui_ctx, event);
	default:
		if (!IMUI_ProcessEvent (imui_ctx, event)) {
			return IN_Binding_HandleEvent (event);
		}
	}
	return 0;
}

@namespace arp {
	static AutoreleasePool *pool;
	static void start (void) { pool = [[AutoreleasePool alloc] init]; }
	static void end (void) { [pool release]; pool = nil; }
}

int
main (int argc, string *argv)
{
	float early_exit = 0;
	if (argc > 1) {
		early_exit = strtof (argv[1], nil);
	}

	arp.start ();

	plitem_t *config = PL_GetPropertyList (render_graph_cfg);
	init_graphics (config, 0, nil);
	PL_Release (config);

	IN_SendConnectedDevices ();

	//Draw_SetScale (1);
	//FIXME need a way to specify font sets (and use them!) because finding
	//nice fonts with all the desired symbols can be quite a chore.
	imui_ctx = IMUI_NewContext ("FreeMono", 22);

	main_menu = [[MainMenu menu:imui_ctx] retain];

	arp.end ();
	arp.start ();

	refresh_2d (draw_2d);
	setevents (event_handler, nil);

	while (!quit_editor) {
		arp.end ();
		arp.start ();

		frametime = refresh (nil);
		realtime += frametime;
		if (early_exit) {
			if (realtime > early_exit + double (1ul<<32)) {
				break;
			}
		}
	}
	[main_menu release];
	arp.end ();
	return 0;
}
