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

static __attribute__ ((unused)) const char rcsid[] = 
	"$Id$";

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
#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
#endif

#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>

#ifdef HAVE_VIDMODE
# include <X11/extensions/xf86vmode.h>
#endif

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/qtypes.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#include "context_x11.h"
#include "dga_check.h"

static void (*event_handlers[LASTEvent]) (XEvent *);
qboolean	oktodraw = false;
int 		x_shmeventtype;

static int	x_disp_ref_count = 0;

Display 	*x_disp = NULL;
int 		x_screen;
Window		x_root = None;
XVisualInfo *x_visinfo;
Visual		*x_vis;
Window		x_win;
Cursor		nullcursor = None;
Time 		x_time;

qboolean    x_have_focus = false;

#define X_MASK (VisibilityChangeMask | StructureNotifyMask | ExposureMask | FocusChangeMask | EnterWindowMask)

#ifdef HAVE_VIDMODE
static XF86VidModeModeInfo **vidmodes;
static int		nummodes;
static int		original_mode = 0;
static vec3_t	x_gamma = {-1, -1, -1};
static qboolean	vidmode_avail = false;
#endif

static qboolean	vidmode_active = false;

static qboolean    vid_context_created = false;
static int  window_x, window_y, window_saved;

static int	xss_timeout;
static int	xss_interval;
static int	xss_blanking;
static int	xss_exposures;

static qboolean	accel_saved = false;
static int	accel_numerator;
static int	accel_denominator;
static int	accel_threshold;


qboolean
X11_AddEvent (int event, void (*event_handler) (XEvent *))
{
	if (event >= LASTEvent) {
		Sys_Printf ("event: %d, LASTEvent: %d\n", event, LASTEvent);
		return false;
	}
	
	if (event_handlers[event])
		return false;

	event_handlers[event] = event_handler;
	return true;
}

qboolean
X11_RemoveEvent (int event, void (*event_handler) (XEvent *))
{
	if (event >= LASTEvent)
		return false;

	if (event_handlers[event] != event_handler)
		return false;

	event_handlers[event] = NULL;
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
		XMaskEvent (x_disp, StructureNotifyMask, &ev);
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
X11_OpenDisplay (void)
{
	if (!x_disp) {
		x_disp = XOpenDisplay (NULL);
		if (!x_disp) {
			Sys_Error ("X11_OpenDisplay: Could not open display [%s]",
					   XDisplayName (NULL));
		}

		x_screen = DefaultScreen (x_disp);
		x_root = RootWindow (x_disp, x_screen);

		XSynchronize (x_disp, true);		// for debugging only

		x_disp_ref_count = 1;
	} else {
		x_disp_ref_count++;
	}
}

void
X11_CloseDisplay (void)
{
	X11_RestoreGamma ();

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
	X11_CreateNullCursor

	Create an empty cursor (in other words, make it disappear)
*/
void
X11_CreateNullCursor (void)
{
	Pixmap		cursormask;
	XGCValues	xgc;
	GC			gc;
	XColor		dummycolour;

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

static void
X11_ForceMove (int x, int y)
{
	int		nx, ny;

	if (!vid_context_created)
		return;

	XMoveWindow (x_disp, x_win, x, y);
	XFlush(x_disp);
	X11_WaitForEvent (ConfigureNotify);
	X11_GetWindowCoords (&nx, &ny);
	nx -= x;
	ny -= y;
	if (nx == 0 || ny == 0) {
		return;
	}
	x -= nx;
	y -= ny;

#if 0 // hopefully this isn't needed!  enable if it is.
	if (x < 1 - scr_width)
		x = 0;
	if (y < 1 - scr_height)
		y = 0;
#endif

	XMoveWindow (x_disp, x_win, x, y);
	XSync (x_disp, false);
	// this is the best we can do.
	X11_WaitForEvent (ConfigureNotify);
}

static vec3_t *
X11_GetGamma (void)
{
#ifdef HAVE_VIDMODE
# ifdef X_XF86VidModeGetGamma
	XF86VidModeGamma	xgamma;
	vec3_t				*temp;

	if (vid_gamma_avail && vid_system_gamma->int_val) {
		if (XF86VidModeGetGamma (x_disp, x_screen, &xgamma)) {
			if ((temp = malloc (sizeof (vec3_t)))) {
				(*temp)[0] = xgamma.red;
				(*temp)[1] = xgamma.green;
				(*temp)[2] = xgamma.blue;
				return temp;
			}
			return NULL;
		}
	}
# endif
#endif
	vid_gamma_avail = false;
	return NULL;
}

void
X11_SetVidMode (int width, int height)
{
	const char *str = getenv ("MESA_GLX_FX");

	if (vidmode_active)
		return;

	if (str && (tolower (*str) == 'f')) {
		Cvar_Set (vid_fullscreen, "1");
	}

#ifdef HAVE_VIDMODE
	{
		static int  initialized = 0;

		if (!vidmode_avail)
			vidmode_avail = VID_CheckVMode (x_disp, NULL, NULL);

		if (!initialized && vidmode_avail) {
			vec3_t	*temp;

			initialized = 1;

			vid_gamma_avail = true;

			temp = X11_GetGamma ();
			if (temp && temp[0] > 0) {
				x_gamma[0] = (*temp)[0];
				x_gamma[1] = (*temp)[1];
				x_gamma[2] = (*temp)[2];
				vid_gamma_avail = true;
				free (temp);
			}

		}

		if (vid_fullscreen->int_val && vidmode_avail) {
			int 				i, dotclock;
			int 				best_mode = 0;
			qboolean			found_mode = false;
			XF86VidModeModeLine orig_data;

			XF86VidModeGetAllModeLines (x_disp, x_screen, &nummodes,
										&vidmodes);
			XF86VidModeGetModeLine (x_disp, x_screen, &dotclock, &orig_data);

			for (i = 0; i < nummodes; i++) {
				if ((vidmodes[i]->hdisplay == orig_data.hdisplay) &&
						(vidmodes[i]->vdisplay == orig_data.vdisplay)) {
					original_mode = i;
					break;
				}
			}

			for (i = 0; i < nummodes; i++) {
				if ((vidmodes[i]->hdisplay == scr_width) &&
						(vidmodes[i]->vdisplay == scr_height)) {
					found_mode = true;
					best_mode = i;
					break;
				}
			}

			if (found_mode) {
				Con_DPrintf ("VID: Chose video mode: %dx%d\n", scr_width,
							 scr_height);

				XF86VidModeSwitchToMode (x_disp, x_screen,
										 vidmodes[best_mode]);
				X11_ForceViewPort ();
				vidmode_active = true;
				X11_SetScreenSaver ();
			} else {
				Con_Printf ("VID: Mode %dx%d can't go fullscreen.\n",
							scr_width, scr_height);
				vidmode_avail = vidmode_active = false;
			}
		}
	}
#endif
}

static void
X11_UpdateFullscreen (cvar_t *fullscreen)
{
	if (!vid_context_created)
		return;

	if (!fullscreen->int_val) {
		X11_RestoreVidMode ();
		if (window_saved) {
			X11_ForceMove (window_x, window_y);
			window_saved = 0;
		}
		IN_UpdateGrab (in_grab);
		return;
	} else {

		if (X11_GetWindowCoords (&window_x, &window_y))
			window_saved = 1;

		X11_SetVidMode (scr_width, scr_height);

		if (!vidmode_active) {
			window_saved = 0;
			return;
		}

		X11_ForceMove (0, 0);
		XWarpPointer (x_disp, None, x_win, 0, 0, 0, 0, scr_width / 2,
					  scr_height / 2);
		IN_UpdateGrab (in_grab);
		// Done in X11_SetVidMode but moved the window since then
		X11_ForceViewPort (); 
	}
}

void
X11_Init_Cvars (void)
{
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE, 
							   &X11_UpdateFullscreen,
							   "Toggles fullscreen game mode");
	vid_system_gamma = Cvar_Get ("vid_system_gamma", "1", CVAR_ARCHIVE, NULL,
								 "Use system gamma control if available");
}
   
void
X11_CreateWindow (int width, int height)
{
	char		   *resname;
	unsigned long	mask;
	XSetWindowAttributes	attr;
	XClassHint	   *ClassHint;
	XSizeHints	   *SizeHints;

	// window attributes
	attr.background_pixel = 0;
	attr.border_pixel = 0;
	attr.colormap = XCreateColormap (x_disp, x_root, x_vis, AllocNone);
	attr.event_mask = X_MASK;
	mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;

	x_win = XCreateWindow (x_disp, x_root, 0, 0, width, height, 0,
						   x_visinfo->depth, InputOutput, x_vis, mask, &attr);

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
	X11_SetCaption (va ("%s %s", PROGRAM, VERSION));

	// Set icon name
	XSetIconName (x_disp, x_win, PROGRAM);

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

	X11_WaitForEvent (MapNotify);

	vid_context_created = true;
	if (vid_fullscreen->int_val) {
		X11_UpdateFullscreen (vid_fullscreen);
	}
	XRaiseWindow (x_disp, x_win);
}

void
X11_RestoreVidMode (void)
{
#ifdef HAVE_VIDMODE
	if (vidmode_active) {
		X11_RestoreScreenSaver ();
		XF86VidModeSwitchToMode (x_disp, x_screen, vidmodes[original_mode]);
		XFree (vidmodes);
		vidmode_active = false;
	}
#endif
}

void
X11_SetCaption (const char *text)
{
	if (x_disp && x_win && text)
		XStoreName (x_disp, x_win, text);
}

qboolean
X11_GetWindowCoords (int *ax, int *ay)
{
#ifdef HAVE_VIDMODE
	Window      	theroot, scrap;
	int         	x, y;
	unsigned int	width, height, bdwidth, depth;

	XSync (x_disp, false);
	if ((XGetGeometry (x_disp, x_win, &theroot, &x, &y, &width, &height,
					   &bdwidth, &depth) == False)) {
		Con_Printf ("XGetWindowAttributes failed in X11_GetWindowCoords.\n");
		return false;
	} else {
		XTranslateCoordinates (x_disp,x_win,theroot, -bdwidth, -bdwidth,
							   ax, ay, &scrap);
		Con_DPrintf ("Window coords =  %dx%d (%d,%d)\n", *ax, *ay,
					 width, height);
		return true;
	}
#endif
	return false;
}

void
X11_ForceViewPort (void)
{
#ifdef HAVE_VIDMODE
	int         ax, ay;

	if (!vidmode_avail  || !vid_context_created)
		return;

	if (!X11_GetWindowCoords (&ax, &ay)) {
		// "icky kludge code"
		Con_Printf ("VID: Falling back on warp kludge to set viewport.\n");
		XWarpPointer (x_disp, None, x_win, 0, 0, 0, 0, scr_width, scr_height);
		XWarpPointer (x_disp, None, x_win, 0, 0, 0, 0, 0, 0);
	}
	XF86VidModeSetViewPort (x_disp, x_screen, ax, ay);
#endif
}

qboolean
X11_SetGamma (double gamma)
{
#ifdef HAVE_VIDMODE
# ifdef X_XF86VidModeSetGamma
	XF86VidModeGamma	xgamma;
	
	if (vid_gamma_avail && vid_system_gamma->int_val && x_have_focus) {
		xgamma.red = xgamma.green = xgamma.blue = (float) gamma;
		if (XF86VidModeSetGamma (x_disp, x_screen, &xgamma))
			return true;
	}
# endif
#endif
	return false;
}

void
X11_RestoreGamma (void)
{
#ifdef HAVE_VIDMODE
# ifdef X_XF86VidModeSetGamma
	XF86VidModeGamma	xgamma;

	if (vid_gamma_avail && x_gamma[0] > 0) {
		xgamma.red = x_gamma[0];
		xgamma.green = x_gamma[1];
		xgamma.blue = x_gamma[2];
		XF86VidModeSetGamma (x_disp, x_screen, &xgamma);
	}
# endif
#endif
}

void
X11_SetScreenSaver (void)
{
	XGetScreenSaver (x_disp, &xss_timeout, &xss_interval, &xss_blanking,
					 &xss_exposures);
	XSetScreenSaver (x_disp, 0, xss_interval, xss_blanking, xss_exposures);
}

void
X11_RestoreScreenSaver (void)
{
	XSetScreenSaver (x_disp, xss_timeout, xss_interval, xss_blanking,
					 xss_exposures);
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
	XChangePointerControl(x_disp, false, false, 0, 0, 0);
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
