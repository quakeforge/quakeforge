/*
	context_x11.c

	general x11 context layer

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Zephaniah E. Hull <warp@whitestar.soark.net>
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <ctype.h>
#include <sys/time.h>
#include <sys/types.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <errno.h>
#include <limits.h>
#include <sys/poll.h>

#ifdef HAVE_VIDMODE
# include <X11/extensions/xf86vmode.h>
#endif

#include "commdef.h"
#include "console.h"
#include "context_x11.h"
#include "cvar.h"
#include "dga_check.h"
#include "input.h"
#include "qargs.h"
#include "qtypes.h"
#include "sys.h"
#include "va.h"
#include "vid.h"

static void (*event_handlers[LASTEvent]) (XEvent *);
qboolean    oktodraw = false;
int         x_shmeventtype;

static int  x_disp_ref_count = 0;

Display    *x_disp = NULL;
int         x_screen;
Window      x_root = None;
XVisualInfo *x_visinfo;
Visual     *x_vis;
Window      x_win;
Cursor      nullcursor = None;
static Atom aWMDelete = 0;

#define X_MASK (VisibilityChangeMask | StructureNotifyMask | ExposureMask)

#ifdef HAVE_VIDMODE
static XF86VidModeModeInfo **vidmodes;
static int  nummodes;
static int	original_mode = 0;
#endif

static qboolean	vidmode_avail = false;
static qboolean	vidmode_active = false;

cvar_t     *vid_fullscreen;
qboolean    vid_fullscreen_active;

static int  xss_timeout;
static int  xss_interval;
static int  xss_blanking;
static int  xss_exposures;

qboolean
x11_add_event (int event, void (*event_handler) (XEvent *))
{
	if (event >= LASTEvent) {
		printf ("event: %d, LASTEvent: %d\n", event, LASTEvent);
		return false;
	}
	if (event_handlers[event] != NULL)
		return false;

	event_handlers[event] = event_handler;
	return true;
}

qboolean
x11_del_event (int event, void (*event_handler) (XEvent *))
{
	if (event >= LASTEvent)
		return false;
	if (event_handlers[event] != event_handler)
		return false;

	event_handlers[event] = NULL;
	return true;
}

void
x11_process_event (void)
{
	XEvent      x_event;

	XNextEvent (x_disp, &x_event);
	if (x_event.type >= LASTEvent) {
		// FIXME: KLUGE!!!!!!
		if (x_event.type == x_shmeventtype)
			oktodraw = 1;
		return;
	}
	if (event_handlers[x_event.type])
		event_handlers[x_event.type] (&x_event);
}

void
x11_process_events (void)
{
	/* Get events from X server. */
	while (XPending (x_disp)) {
		x11_process_event ();
	}
}

// ========================================================================
// Tragic death handler
// ========================================================================

static void
TragicDeath (int sig)
{
	printf ("Received signal %d, exiting...\n", sig);
	Sys_Quit ();
	exit (sig);
	// XCloseDisplay(x_disp);
	// VID_Shutdown();
	// Sys_Error("This death brought to you by the number %d\n", signal_num);
}

void
x11_open_display (void)
{
	if (!x_disp) {
		x_disp = XOpenDisplay (NULL);
		if (!x_disp) {
			Sys_Error ("x11_open_display: Could not open display [%s]\n",
					   XDisplayName (NULL));
		}

		x_screen = DefaultScreen (x_disp);
		x_root = RootWindow (x_disp, x_screen);

		// catch signals
		signal (SIGHUP, TragicDeath);
		signal (SIGINT, TragicDeath);
		signal (SIGQUIT, TragicDeath);
		signal (SIGILL, TragicDeath);
		signal (SIGTRAP, TragicDeath);
		signal (SIGIOT, TragicDeath);
		signal (SIGBUS, TragicDeath);
		/* signal(SIGFPE, TragicDeath); */
		signal (SIGSEGV, TragicDeath);
		signal (SIGTERM, TragicDeath);

		// for debugging only
		XSynchronize (x_disp, True);

		x_disp_ref_count = 1;
	} else {
		x_disp_ref_count++;
	}
}

void
x11_close_display (void)
{
	if (nullcursor != None) {
		XFreeCursor (x_disp, nullcursor);
		nullcursor = None;
	}

	if (!--x_disp_ref_count) {
		XCloseDisplay (x_disp);
		x_disp = 0;
	}
}

/*
	x11_create_null_cursor

	Create an empty cursor
*/
void
x11_create_null_cursor (void)
{
	Pixmap      cursormask;
	XGCValues   xgc;
	GC          gc;
	XColor      dummycolour;

	if (nullcursor != None)
		return;

	cursormask = XCreatePixmap (x_disp, x_root, 1, 1, 1);
	xgc.function = GXclear;

	gc = XCreateGC (x_disp, cursormask, GCFunction, &xgc);

	XFillRectangle (x_disp, cursormask, gc, 0, 0, 1, 1);

	dummycolour.pixel = 0;
	dummycolour.red = 0;
	dummycolour.flags = 04;
	nullcursor = XCreatePixmapCursor (x_disp, cursormask, cursormask,
									  &dummycolour, &dummycolour, 0, 0);
	XFreePixmap (x_disp, cursormask);
	XFreeGC (x_disp, gc);
	XDefineCursor (x_disp, x_win, nullcursor);
}

void
x11_set_vidmode (int width, int height)
{
	const char *str = getenv ("MESA_GLX_FX");

	if (str && (tolower (*str) == 'f')) {
		Cvar_Set (vid_fullscreen, "1");
	}

	XGetScreenSaver (x_disp, &xss_timeout, &xss_interval, &xss_blanking,
					 &xss_exposures);

#ifdef HAVE_VIDMODE
	vidmode_avail = VID_CheckVMode (x_disp, NULL, NULL);

	if (vid_fullscreen->int_val && vidmode_avail) {

		int 				i, dotclock;
		int 				best_mode = 0;
		qboolean			found_mode = false;
		XF86VidModeModeLine orig_data;

		XF86VidModeGetAllModeLines (x_disp, x_screen, &nummodes, &vidmodes);
		XF86VidModeGetModeLine (x_disp, x_screen, &dotclock, &orig_data);

		for (i = 0; i < nummodes; i++) {
			if ((vidmodes[i]->hdisplay == orig_data.hdisplay) &&
					(vidmodes[i]->vdisplay == orig_data.vdisplay)) {
				original_mode = i;
				break;
			}
		}

		for (i = 0; i < nummodes; i++) {
			if ((vidmodes[i]->hdisplay == vid.width) &&
					(vidmodes[i]->vdisplay == vid.height)) {
				found_mode = true;
				best_mode = i;
				break;
			}
		}

		if (found_mode) {
			Con_Printf ("VID: Chose video mode: %dx%d\n", vid.width, vid.height);

			XSetScreenSaver (x_disp, 0, xss_interval, xss_blanking, xss_exposures);
			XF86VidModeSwitchToMode (x_disp, x_screen, vidmodes[best_mode]);
			x11_force_view_port ();
			vidmode_active = true;
		} else {
			Con_Printf ("VID: Mode %dx%d can't go fullscreen.\n", vid.width, vid.height);
			vidmode_avail = vidmode_active = false;
		}
	}
#endif
}

void
x11_Init_Cvars (void)
{
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ROM,
							   "Toggles fullscreen game mode");
}

void
x11_create_window (int width, int height)
{
	XSetWindowAttributes attr;
	XClassHint *ClassHint;
	XSizeHints *SizeHints;
	char       *resname;
	unsigned long mask;

	/* window attributes */
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap (x_disp, x_root, x_vis, AllocNone);
	attr.event_mask = X_MASK;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	if (vidmode_active && vid_fullscreen->int_val) {
		attr.override_redirect = 1;
		mask |= CWOverrideRedirect;
	}

	x_win = XCreateWindow (x_disp, x_root, 0, 0, width, height,
						   0, x_visinfo->depth, InputOutput,
						   x_vis, mask, &attr);

	// Set window size hints
	SizeHints = XAllocSizeHints ();
	if (SizeHints) {
		SizeHints->flags = (PMinSize | PMaxSize);
		SizeHints->min_width = width;
		SizeHints->min_height = height;
		SizeHints->max_width = width;
		SizeHints->max_height = height;
		XSetWMNormalHints (x_disp, x_win, SizeHints);

		XFree (SizeHints);
	}
	// Set window title
	x11_set_caption (va ("%s %s", PROGRAM, VERSION));

	// Set icon name
	XSetIconName (x_disp, x_win, PROGRAM);

	// Set window class
	ClassHint = XAllocClassHint ();
	if (ClassHint) {
		resname = strrchr (com_argv[0], '/');

		ClassHint->res_name = (resname ? resname + 1 : com_argv[0]);
		ClassHint->res_class = PACKAGE;
		XSetClassHint (x_disp, x_win, ClassHint);
		XFree (ClassHint);
	}
	// Make window respond to Delete events
	aWMDelete = XInternAtom (x_disp, "WM_DELETE_WINDOW", False);
	XSetWMProtocols (x_disp, x_win, &aWMDelete, 1);

	if (vidmode_active && vid_fullscreen->int_val) {
		XMoveWindow (x_disp, x_win, 0, 0);
		XWarpPointer (x_disp, None, x_win, 0, 0, 0, 0,
					  vid.width + 2, vid.height + 2);
		x11_force_view_port ();
	}

	XMapWindow (x_disp, x_win);
	if (vidmode_active && vid_fullscreen->int_val) {
		XGrabPointer (x_disp, x_win, True, 0, GrabModeAsync, GrabModeAsync, x_win, None, CurrentTime);
	}
	XRaiseWindow (x_disp, x_win);
}

void
x11_restore_vidmode (void)
{
	XSetScreenSaver (x_disp, xss_timeout, xss_interval, xss_blanking,
					 xss_exposures);

#ifdef HAVE_VIDMODE
	if (vidmode_active) {
		XF86VidModeSwitchToMode (x_disp, x_screen, vidmodes[original_mode]);
		XFree (vidmodes);
	}
#endif
}

void
x11_grab_keyboard (void)
{
#ifdef HAVE_VIDMODE
	if (vidmode_active && vid_fullscreen->int_val) {
		XGrabKeyboard (x_disp, x_win, 1, GrabModeAsync, GrabModeAsync,
					   CurrentTime);
	}
#endif
}

void
x11_set_caption (char *text)
{
	if (x_disp && x_win && text)
		XStoreName (x_disp, x_win, text);
}

void
x11_force_view_port (void)
{
#ifdef HAVE_VIDMODE
	int         x, y;

	if (vidmode_active && vid_fullscreen->int_val) {
		do {
			XF86VidModeSetViewPort (x_disp, x_screen, 0, 0);
			poll (0, 0, 50);
			XF86VidModeGetViewPort (x_disp, x_screen, &x, &y);
		} while (x || y);
	}
#endif
}
