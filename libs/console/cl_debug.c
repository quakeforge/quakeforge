#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "QF/keys.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/input/event.h"

#include "QF/ui/canvas.h"
#include "QF/ui/font.h"
#include "QF/ui/imui.h"
#include "QF/ui/view.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "cl_console.h"

static int debug_event_id;
static int debug_saved_focus;
static imui_ctx_t *debug_imui;
static int64_t  debug_enable_time;
#define IMUI_context debug_imui

bool con_debug;
static cvar_t con_debug_cvar = {
	.name = "con_debug",
	.description =
		"Enable the debug interface",
	.default_value = "false",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_bool, .value = &con_debug },
};
static char *deb_fontname;
static cvar_t deb_fontname_cvar = {
	.name = "deb_fontname",
	.description =
		"Font name/patter for debug interface",
	.default_value = "Consolas",
	.flags = CVAR_NONE,
	.value = { .type = 0, .value = &deb_fontname },
};
static float deb_fontsize;
static cvar_t deb_fontsize_cvar = {
	.name = "deb_fontsize",
	.description =
		"Font size for debug interface",
	.default_value = "22",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &deb_fontsize },
};

static int deb_xlen = -1;
static int deb_ylen = -1;

static void
con_debug_f (void *data, const cvar_t *cvar)
{
	Con_Show_Mouse (con_debug);
	if (debug_imui) {
		IMUI_SetVisible (debug_imui, con_debug);
		debug_enable_time = Sys_LongTime ();
		if (!con_debug) {
			IE_Set_Focus (debug_saved_focus);
		} else {
			IMUI_SetSize (debug_imui, deb_xlen, deb_ylen);
		}
	}
}

static void
debug_app_window (const IE_event_t *ie_event)
{
	if (deb_xlen != ie_event->app_window.xlen
		|| deb_ylen != ie_event->app_window.ylen) {
		deb_xlen = ie_event->app_window.xlen;
		deb_ylen = ie_event->app_window.ylen;
		IMUI_SetSize (debug_imui, deb_xlen, deb_ylen);
	}
}

static void
debug_mouse (const IE_event_t *ie_event)
{
	IMUI_ProcessEvent (debug_imui, ie_event);
}

static void
close_debug (void)
{
	con_debug = false;
	con_debug_f (0, &con_debug_cvar);
}

static void
debug_key (const IE_event_t *ie_event)
{
	int shift = ie_event->key.shift & ~(ies_capslock | ies_numlock);
	if (ie_event->key.code == QFK_ESCAPE && shift == ies_control) {
		close_debug ();
	}
	IMUI_ProcessEvent (debug_imui, ie_event);
}

static int
debug_event_handler (const IE_event_t *ie_event, void *data)
{
	static void (*handlers[ie_event_count]) (const IE_event_t *ie_event) = {
		[ie_app_window] = debug_app_window,
		[ie_mouse] = debug_mouse,
		[ie_key] = debug_key,
	};
	if ((unsigned) ie_event->type >= ie_event_count
		|| !handlers[ie_event->type]) {
		return 0;
	}
	handlers[ie_event->type] (ie_event);
	return 1;
}

void
Con_Debug_InitCvars (void)
{
	Cvar_Register (&con_debug_cvar, con_debug_f, 0);
	Cvar_Register (&deb_fontname_cvar, 0, 0);
	Cvar_Register (&deb_fontsize_cvar, 0, 0);
}

static imui_style_t current_style;
void
Con_Debug_Init (void)
{
	debug_event_id = IE_Add_Handler (debug_event_handler, 0);
	debug_imui = IMUI_NewContext (*con_data.canvas_sys,
								  deb_fontname, deb_fontsize);
	IMUI_SetVisible (debug_imui, con_debug);
	IMUI_Style_Fetch (debug_imui, &current_style);
}

void
Con_Debug_Shutdown (void)
{
	IE_Remove_Handler (debug_event_id);
}

static imui_window_t style_editor = {
	.name = "Style Colors",
	.xpos = 75,
	.ypos = 75,
	.xlen = 400,
	.ylen = 300,
	.is_open = true,
};
static int style_selection;
static int style_mode;
static int style_color;

static void
color_window (void)
{
	UI_Window (&style_editor) {
		if (!style_editor.is_open || style_editor.is_collapsed) {
			continue;
		}
		IMUI_Layout_SetXSize (debug_imui, imui_size_fitchildren, 0);
		IMUI_Layout_SetYSize (debug_imui, imui_size_fitchildren, 0);
		UI_Vertical {
			UI_Horizontal {
				UI_Radio (&style_selection, 0, "Background");
				UI_Radio (&style_selection, 1, "Foreground");
				UI_Radio (&style_selection, 2, "Text");
				UI_FlexibleSpace ();
			}

			UI_Horizontal {
				UI_Radio (&style_mode, 0, "Normal");
				UI_Radio (&style_mode, 1, "Hot");
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
			IMUI_Style_Fetch (debug_imui, &style);
			auto bg_save = style.background.normal;
			auto fg_save = style.foreground.normal;
			UI_Style (0) for (int y = 0; y < 16; y++) {
				UI_Horizontal for (int x = 0; x < 16; x++) {
					if (!x) UI_FlexibleSpace ();
					int c = y * 16 + x;
					int ic = y * 16 + (15-x);
					style.background.normal = c;
					style.foreground.normal = c == style_color ? c : ic;
					IMUI_Style_Update (debug_imui, &style);
					UI_Radio (&style_color, c, va (0, "##color_%x%x", y, x));
					if (x == 15) {
						style.background.normal = bg_save;
						style.foreground.normal = fg_save;
						IMUI_Style_Update (debug_imui, &style);
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

void
Con_Debug_Draw (void)
{
	if (debug_enable_time && Sys_LongTime () - debug_enable_time > 1000) {
		debug_saved_focus = IE_Get_Focus ();
		IE_Set_Focus (debug_event_id);
		debug_enable_time = 0;
	}

	IMUI_BeginFrame (debug_imui);
	IMUI_Style_Update (debug_imui, &current_style);
static int state;
static bool flag = true;
static imui_window_t window = {
	.name = "Test Window",
	.xpos = 50,
	.ypos = 50,
	.xlen = 400,
	.ylen = 250,
	.is_open = true,
};
	bool close_debug_pressed = false;
	UI_Window (&window) {
		if (!window.is_open || window.is_collapsed) {
			continue;
		}
		UI_Vertical {
			UI_Horizontal {
				if (UI_Button ("Close Debug")) {
					close_debug_pressed = true;
				}
				UI_FlexibleSpace ();
				if (flag) {
					UI_Label ("abcde##1");
				}
			}
			UI_Horizontal {
				UI_Checkbox (&flag, "hi there");
				UI_FlexibleSpace ();
				if (flag) {
					UI_Label ("abcdefg##2");
				}
			}
			UI_Horizontal {
				UI_Radio (&state, 0, "A");
				UI_Radio (&state, 1, "B");
				UI_Radio (&state, 2, "C");
				UI_FlexibleSpace ();
			}
			UI_Horizontal {
				UI_Label (va(0, "mem: %zd", Sys_CurrentRSS ()));
				UI_FlexibleSpace ();
			}
			UI_FlexibleSpace ();
		}
	}
	color_window ();

	if (close_debug_pressed) {
		close_debug ();
	}

	IMUI_Draw (debug_imui);
}
