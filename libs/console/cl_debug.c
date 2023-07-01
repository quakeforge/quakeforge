#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"

#include "QF/input/event.h"

#include "QF/ui/canvas.h"
#include "QF/ui/font.h"
#include "QF/ui/imui.h"
#include "QF/ui/view.h"

#include "QF/plugin/console.h"
#include "QF/plugin/vid_render.h"

#include "cl_console.h"

static int debug_event_id;
static imui_ctx_t *debug_imui;
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

static int
debug_event_handler (const IE_event_t *ie_event, void *data)
{
	static void (*handlers[ie_event_count]) (const IE_event_t *ie_event) = {
		[ie_app_window] = debug_app_window,
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
	UI_Button ("Hi there!");

	IMUI_Draw (debug_imui);
}
