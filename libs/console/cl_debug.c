#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "QF/keys.h"
#include "QF/sys.h"

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

static void
con_debug_f (void *data, const cvar_t *cvar)
{
	Con_Show_Mouse (con_debug);
	if (debug_imui) {
		IMUI_SetVisible (debug_imui, con_debug);
		debug_enable_time = Sys_LongTime ();
		if (!con_debug) {
			IE_Set_Focus (debug_saved_focus);
		}
	}
}

static int deb_xlen = -1;
static int deb_ylen = -1;
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

void
Con_Debug_Init (void)
{
	debug_event_id = IE_Add_Handler (debug_event_handler, 0);
	debug_imui = IMUI_NewContext (*con_data.canvas_sys,
								  deb_fontname, deb_fontsize);
	IMUI_SetVisible (debug_imui, con_debug);
}

void
Con_Debug_Shutdown (void)
{
	IE_Remove_Handler (debug_event_id);
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
static int state;
static bool flag = true;
	UI_Layout(true) {
		UI_Layout(false) {
			if (UI_Button ("Close Debug")) {
				close_debug ();
			}
			if (flag) {
				UI_Button ("_##1");
			}
		}
		UI_Layout(false) {
			UI_Checkbox (&flag, "hi there");
			if (flag) {
				UI_Button ("_##2");
			}
		}
		UI_Layout(false) {
			UI_Radio (&state, 0, "A");
			UI_Radio (&state, 1, "B");
			UI_Radio (&state, 2, "C");
		}
	}

	IMUI_Draw (debug_imui);
}
