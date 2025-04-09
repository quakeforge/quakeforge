/*
	context_x11.c

	general x11 context layer

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Zephaniah E. Hull <warp@whitestar.soark.net>
	Copyright (C) 2000, 2001 Jeff Teunissen <deek@quakeforge.net>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <ctype.h>
#include <sys/time.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <X11/Xlib.h>
#ifdef HAVE_VIDMODE
# include <X11/extensions/xf86vmode.h>
#endif
#ifdef HAVE_XSS
# include <X11/extensions/scrnsaver.h>
#endif

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/qtypes.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "QF/input/event.h"

#include "context_x11.h"
#include "dga_check.h"
#include "in_x11.h"
#include "vid_internal.h"

static void (*event_handlers[LASTEvent]) (XEvent *);
bool		oktodraw = false;
int 		x_shmeventtype;

static int	x_disp_ref_count = 0;
static bool x_have_xss = false;

Display 	*x_disp = nullptr;
int 		x_screen;
int			x_width;
int			x_height;
Window		x_root = None;
XVisualInfo *x_visinfo;
Visual		*x_vis;
Window		x_win;
Colormap	x_cmap;
Time 		x_time;
Time		x_mouse_time;
Cursor      x_nullcursor = None;

bool        x_have_focus = false;

#ifdef HAVE_VIDMODE
static XF86VidModeModeInfo **vidmodes;
static int		nummodes;
static int		original_mode = 0;
static vec3_t	x_gamma = {-1, -1, -1};
#endif
static bool		vidmode_avail = false;

static bool		vidmode_active = false;

static bool        vid_context_created = false;
static int      pos_x, pos_y;

#ifdef HAVE_VIDMODE
static int	xss_timeout;
static int	xss_interval;
static int	xss_blanking;
static int	xss_exposures;
#endif

static bool		accel_saved = false;
static int	accel_numerator;
static int	accel_denominator;
static int	accel_threshold;

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2
static Atom x_net_state;
static Atom x_net_fullscreen;

static int x11_vidmode;
static cvar_t x11_vidmode_cvar = {
	.name = "x11_vidmode",
	.description =
		"Use x11 vidmode extension to set video mode (not recommended for "
		"modern systems)",
	.default_value = "0",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &x11_vidmode },
};

static void
set_fullscreen (int full)
{
	XEvent      xev;

	xev.xclient.type = ClientMessage;
	xev.xclient.serial = 0;
	xev.xclient.send_event = True;
	xev.xclient.window = x_win;
	xev.xclient.message_type = x_net_state;
	xev.xclient.format = 32;

	if (full)
		xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
	else
		xev.xclient.data.l[0] = _NET_WM_STATE_REMOVE;

	xev.xclient.data.l[1] = x_net_fullscreen;
	xev.xclient.data.l[2] = 0;
	xev.xclient.data.l[3] = 0;
	xev.xclient.data.l[4] = 0;
	XSendEvent (x_disp, x_root, False,
				SubstructureRedirectMask | SubstructureNotifyMask, &xev);
}

static void
configure_notify (XEvent *event)
{
	XConfigureEvent *c = &event->xconfigure;
	pos_x = c->x;
	pos_y = c->y;
#ifdef HAVE_VIDMODE
	if (vidmode_active)
		X11_ForceViewPort ();
#endif
	Sys_MaskPrintf (SYS_vid,
					"ConfigureNotify: %ld %d %ld %ld %d,%d (%d,%d) "
					"%d %ld %d\n",
					c->serial, c->send_event, c->event, c->window, c->x, c->y,
					c->width, c->height, c->border_width, c->above,
					c->override_redirect);
	VID_SetWindow (c->x, c->y, c->width, c->height);
}

bool
X11_AddEvent (int event, void (*event_handler) (XEvent *))
{
	if (event >= LASTEvent) {
		Sys_MaskPrintf (SYS_vid, "event: %d, LASTEvent: %d\n", event,
						LASTEvent);
		return false;
	}

	if (event_handlers[event])
		return false;

	event_handlers[event] = event_handler;
	return true;
}

bool
X11_RemoveEvent (int event, void (*event_handler) (XEvent *))
{
	if (event >= LASTEvent)
		return false;

	if (event_handlers[event] != event_handler)
		return false;

	event_handlers[event] = nullptr;
	return true;
}

static inline void
X11_ProcessEventProxy(XEvent *x_event)
{
	if (x_event->type >= LASTEvent) {
		if (x_event->type == x_shmeventtype)
			oktodraw = 1;
		return;
	}
	if (event_handlers[x_event->type])
		event_handlers[x_event->type] (x_event);
}

static void
X11_WaitForEvent (int event)
{
	XEvent	ev;
	int     type;

	while (1) {
		XMaskEvent (x_disp, X11_WINDOW_MASK, &ev);
		type = ev.type;
		X11_ProcessEventProxy (&ev);
		if (type == event)
			break;
	}
}

inline void
X11_ProcessEvent (void)
{
	XEvent      x_event;

	XNextEvent (x_disp, &x_event);
	X11_ProcessEventProxy (&x_event);
}

void
X11_ProcessEvents (void)
{
	while (XPending (x_disp))
		X11_ProcessEvent ();
}

void
X11_SetScreenSaver (void)
{
	if (x_have_xss) {
#ifdef HAVE_XSS
		XScreenSaverSuspend (x_disp, true);
#endif
	} else {
#ifdef HAVE_VIDMODE
		XGetScreenSaver (x_disp, &xss_timeout, &xss_interval, &xss_blanking,
						 &xss_exposures);
		XSetScreenSaver (x_disp, 0, xss_interval, xss_blanking, xss_exposures);
#endif
	}
}

void
X11_RestoreScreenSaver (void)
{
	if (x_have_xss) {
#ifdef HAVE_XSS
		XScreenSaverSuspend (x_disp, false);
#endif
	} else {
#ifdef HAVE_VIDMODE
		XSetScreenSaver (x_disp, xss_timeout, xss_interval, xss_blanking,
						 xss_exposures);
#endif
	}
}

static void
X11_CheckScreenSaver (void)
{
#ifdef HAVE_XSS
	int ev_base, err_base;
	int maj_ver, min_ver;
	if (!XScreenSaverQueryExtension (x_disp, &ev_base, &err_base)) {
		Sys_MaskPrintf (SYS_vid, "XSS not supported\n");
		return;
	}
	if (!XScreenSaverQueryVersion (x_disp, &maj_ver, &min_ver)) {
		Sys_MaskPrintf (SYS_vid, "XScreenSaver version query failed\n");
		return;
	}
	Sys_MaskPrintf (SYS_vid, "XSS supperted: version %d.%d (ev:%d, err:%d)\n",
					maj_ver, min_ver, ev_base, err_base);
	x_have_xss = true;
#else
	Sys_MaskPrintf (SYS_vid, "X11: XScreenSaver support not compiled in\n");
#endif
}

void
X11_OpenDisplay (void)
{
	qfZoneScoped (true);
	if (!x_disp) {
		x_disp = XOpenDisplay (nullptr);
		if (!x_disp) {
			Sys_Error ("X11_OpenDisplay: Could not open display [%s]",
					   XDisplayName (nullptr));
		}

		X11_CheckScreenSaver ();

		x_net_state = XInternAtom (x_disp, "_NET_WM_STATE", False);
		x_net_fullscreen = XInternAtom (x_disp, "_NET_WM_STATE_FULLSCREEN",
										False);

		x_screen = DefaultScreen (x_disp);
		x_width = DisplayWidth (x_disp, x_screen);
		x_height = DisplayHeight (x_disp, x_screen);
		x_root = RootWindow (x_disp, x_screen);

		XSynchronize (x_disp, true);		// only for debugging

		x_disp_ref_count = 1;
	} else {
		x_disp_ref_count++;
	}
}

void
X11_CloseDisplay (void)
{
	if (!--x_disp_ref_count) {
		X11_RestoreVidMode ();

		X11_RestoreGamma ();
		X11_RestoreScreenSaver ();

		if (x_nullcursor != None) {
			XFreeCursor (x_disp, x_nullcursor);
			x_nullcursor = None;
		}

		XCloseDisplay (x_disp);
		x_disp = 0;
	}
}

/*
	X11_CreateNullCursor

	Create an empty cursor (in other words, make it disappear)
*/
void
X11_CreateNullCursor (void)
{
	qfZoneScoped (true);
	Pixmap		cursormask;
	XGCValues	xgc = { };
	GC			gc;
	XColor		dummycolour = { };

	if (x_nullcursor != None)
		return;

	cursormask = XCreatePixmap (x_disp, x_root, 1, 1, 1);

	xgc.function = GXclear;
	gc = XCreateGC (x_disp, cursormask, GCFunction, &xgc);

	XFillRectangle (x_disp, cursormask, gc, 0, 0, 1, 1);

	dummycolour.pixel = 0;
	dummycolour.red = 0;
	dummycolour.flags = 04;
	x_nullcursor = XCreatePixmapCursor (x_disp, cursormask, cursormask,
									  &dummycolour, &dummycolour, 0, 0);
	XFreePixmap (x_disp, cursormask);
	XFreeGC (x_disp, gc);
}

#ifdef HAVE_VIDMODE
static vec3_t *
X11_GetGamma (void)
{
	XF86VidModeGamma	xgamma;
	vec3_t				*temp;

	if (vid_gamma_avail && vid_system_gamma) {
		if (XF86VidModeGetGamma (x_disp, x_screen, &xgamma)) {
			if ((temp = malloc (sizeof (vec3_t)))) {
				(*temp)[0] = xgamma.red;
				(*temp)[1] = xgamma.green;
				(*temp)[2] = xgamma.blue;
				return temp;
			}
			return nullptr;
		}
	}
	vid_gamma_avail = false;
	return nullptr;
}
#endif

void
X11_SetVidMode (int width, int height)
{
	qfZoneScoped (true);
	const char *str = getenv ("MESA_GLX_FX");

	if (vidmode_active)
		return;

	if (!x11_vidmode) {
		return;
	}

	if (str && (tolower (*str) == 'f')) {
		Cvar_Set ("vid_fullscreen", "1");
	}

#ifdef HAVE_VIDMODE
	{
		static int  initialized = 0;

		if (!vidmode_avail)
			vidmode_avail = VID_CheckVMode (x_disp, nullptr, nullptr);

		if (!initialized && vidmode_avail) {
			vec3_t	*temp;

			initialized = 1;

			vid_gamma_avail = true;

			temp = X11_GetGamma ();
			if (temp && (*temp)[0] > 0) {
				x_gamma[0] = (*temp)[0];
				x_gamma[1] = (*temp)[1];
				x_gamma[2] = (*temp)[2];
				vid_gamma_avail = true;
				free (temp);
			}

		}

		if (vid_fullscreen && vidmode_avail) {
			int 				i, dotclock;
			int 				best_mode = 0;
			bool				found_mode = false;
			XF86VidModeModeLine orig_data;

			XF86VidModeGetAllModeLines (x_disp, x_screen, &nummodes,
										&vidmodes);
			XF86VidModeGetModeLine (x_disp, x_screen, &dotclock, &orig_data);

			Sys_MaskPrintf (SYS_vid, "VID: %d modes\n", nummodes);
			original_mode = -1;
			for (i = 0; i < nummodes; i++) {
				if (original_mode == -1
					&& (vidmodes[i]->hdisplay == orig_data.hdisplay) &&
					   (vidmodes[i]->vdisplay == orig_data.vdisplay)) {
					original_mode = i;
				}
				if (developer & SYS_vid) {
					Sys_Printf ("VID:%c%dx%d\n",
								original_mode == i ? '*' : ' ',
								vidmodes[i]->hdisplay, vidmodes[i]->vdisplay);
					Sys_Printf ("\t%d %d %d %d:%d %d %d:%d\n",
								vidmodes[i]->hsyncstart, vidmodes[i]->hsyncend,
								vidmodes[i]->htotal, vidmodes[i]->hskew,
								vidmodes[i]->vsyncstart, vidmodes[i]->vsyncend,
								vidmodes[i]->vtotal, vidmodes[i]->flags);
				}
			}

			for (i = 0; i < nummodes; i++) {
				if ((vidmodes[i]->hdisplay == viddef.width) &&
						(vidmodes[i]->vdisplay == viddef.height)) {
					found_mode = true;
					best_mode = i;
					break;
				}
			}

			if (found_mode) {
				Sys_MaskPrintf (SYS_vid, "VID: Chose video mode: %dx%d\n",
								viddef.width, viddef.height);

				if (0) {
				XF86VidModeSwitchToMode (x_disp, x_screen,
										 vidmodes[best_mode]);
				}
				vidmode_active = true;
			} else {
				Sys_Printf ("VID: Mode %dx%d can't go fullscreen.\n",
							viddef.width, viddef.height);
				vidmode_avail = vidmode_active = false;
			}
		}
	}
#endif
}

void
X11_UpdateFullscreen (int fullscreen)
{
	if (!vid_context_created)
		return;

	if (!fullscreen) {
		X11_RestoreVidMode ();
		set_fullscreen (0);
		IN_UpdateGrab (in_grab);
	} else {
		set_fullscreen (1);
		X11_SetVidMode (viddef.width, viddef.height);
		IN_UpdateGrab (in_grab);
	}
}

static void
VID_Center_f (void)
{
	X11_ForceViewPort ();
}


void
X11_Init_Cvars (void)
{
	Cmd_AddCommand ("vid_center", VID_Center_f, "Center the view port on the "
					"quake window in a virtual desktop.\n");
	Cvar_Register (&x11_vidmode_cvar, 0, 0);
}

void
X11_CreateWindow (int width, int height)
{
	qfZoneScoped (true);
	char		   *resname;
	unsigned long	mask;
	XSetWindowAttributes	attr;
	XClassHint	   *ClassHint;

	X11_AddEvent (ConfigureNotify, configure_notify);

	// window attributes
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = x_cmap;
	if (!attr.colormap) {
		attr.colormap = XCreateColormap (x_disp, x_root, x_vis, AllocNone);
	}
	attr.event_mask = X11_MASK | IN_X11_Preinit ();

	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;


	x_win = XCreateWindow (x_disp, x_root, 0, 0, width, height, 0,
						   x_visinfo->depth, InputOutput, x_vis, mask, &attr);
	IN_X11_Postinit ();

	// Set window size hints
	XSizeHints	    SizeHints = {
		.flags = PMinSize,
		.min_width = 32,
		.min_height = 20,
	};
	XSetWMNormalHints (x_disp, x_win, &SizeHints);

	// Set window title
	X11_SetCaption (va ("%s", PACKAGE_STRING));

	// Set icon name
	XSetIconName (x_disp, x_win, PACKAGE_NAME);

	// Set window class
	ClassHint = XAllocClassHint ();
	if (ClassHint) {
		resname = strrchr (com_argv[0], '/');

		ClassHint->res_name = (char *)(resname ? resname + 1 : com_argv[0]);
		ClassHint->res_class = (char *)PACKAGE;
		XSetClassHint (x_disp, x_win, ClassHint);
		XFree (ClassHint);
	}

	XMapWindow (x_disp, x_win);

	X11_WaitForEvent (ConfigureNotify);

	vid_context_created = true;
	XRaiseWindow (x_disp, x_win);
	X11_WaitForEvent (VisibilityNotify);
	X11_UpdateFullscreen (vid_fullscreen);
}

void
X11_RestoreVidMode (void)
{
	if (vidmode_active) {
#ifdef HAVE_VIDMODE
		//XF86VidModeSwitchToMode (x_disp, x_screen, vidmodes[original_mode]);
		XFree (vidmodes);
		vidmode_active = false;
#endif
	}
}

void
X11_SetCaption (const char *text)
{
	if (x_disp && x_win && text)
		XStoreName (x_disp, x_win, text);
}

void
X11_ForceViewPort (void)
{
#ifdef HAVE_VIDMODE
	if (!vidmode_avail  || !vid_context_created)
		return;

	XF86VidModeSetViewPort (x_disp, x_screen, pos_x, pos_y);
#endif
}

bool
X11_SetGamma (double gamma)
{
#ifdef HAVE_VIDMODE
	XF86VidModeGamma	xgamma;

	if (vid_gamma_avail && vid_system_gamma && x_have_focus) {
		xgamma.red = xgamma.green = xgamma.blue = (float) gamma;
		if (XF86VidModeSetGamma (x_disp, x_screen, &xgamma))
			return true;
	}
#else
	Sys_MaskPrintf (SYS_vid, "X11_SetGamma: cannot adjust gamma\n");
#endif
	return false;
}

void
X11_RestoreGamma (void)
{
#ifdef HAVE_VIDMODE
	XF86VidModeGamma	xgamma;

	if (vid_gamma_avail && x_gamma[0] > 0) {
		xgamma.red = x_gamma[0];
		xgamma.green = x_gamma[1];
		xgamma.blue = x_gamma[2];
		XF86VidModeSetGamma (x_disp, x_screen, &xgamma);
	}
#else
	Sys_MaskPrintf (SYS_vid, "X11_RestoreGamma: cannot adjust gamma\n");
#endif
}


void
X11_SaveMouseAcceleration (void)
{
	accel_saved = true;
	XGetPointerControl(x_disp, &accel_numerator, &accel_denominator,
					   &accel_threshold);
}

void
X11_RemoveMouseAcceleration (void)
{
	XChangePointerControl(x_disp, true, false, 0, 1, 0);
}

void
X11_RestoreMouseAcceleration (void)
{
	if (!accel_saved)
		return;

	XChangePointerControl(x_disp, true, true, accel_numerator,
						  accel_denominator, accel_threshold);
	accel_saved = false;
}

extern int x11_force_link;
static __attribute__((used)) int *context_x11_force_link = &x11_force_link;
