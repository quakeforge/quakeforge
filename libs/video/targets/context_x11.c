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

#ifdef HAVE_EXECINFO_H
# include <execinfo.h>
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

#ifdef HAVE_VIDMODE
# include <X11/extensions/xf86vmode.h>
#endif

#include "QF/console.h"
#include "context_x11.h"
#include "QF/cvar.h"
#include "dga_check.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/qtypes.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

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
static Atom aWMDelete = 0;

#define X_MASK (VisibilityChangeMask | StructureNotifyMask | ExposureMask)
#define MOUSE_MASK (ButtonPressMask | ButtonReleaseMask | PointerMotionMask)

#ifdef HAVE_VIDMODE
static XF86VidModeModeInfo **vidmodes;
static int	nummodes;
static int	original_mode = 0;
static double  x_gamma;
static qboolean vidmode_avail = false;
#endif

static qboolean	vidmode_active = false;

cvar_t		*vid_fullscreen;
cvar_t		*vid_system_gamma;
qboolean	vid_fullscreen_active;
static qboolean    vid_context_created = false;
static int  window_x, window_y, window_saved;

cvar_t		*sys_backtrace;
cvar_t		*sys_dump_core;

static int	xss_timeout;
static int	xss_interval;
static int	xss_blanking;
static int	xss_exposures;

void
dump_core_callback (cvar_t *sys_dump_core)
{
#ifndef HAVE_UNISTD_H
	if (sys_dump_core->int_val)
		Con_Printf ("support for dumping core has not been compiled in!\n");
#endif
}

void
backtrace_callback (cvar_t *sys_backtrace)
{
#ifndef HAVE_EXECINFO_H
	if (sys_backtrace->int_val)
		Con_Printf ("support for printing backtraces has not been compiled in!\n");
#endif
}

qboolean
X11_AddEvent (int event, void (*event_handler) (XEvent *))
{
	if (event >= LASTEvent) {
		printf ("event: %d, LASTEvent: %d\n", event, LASTEvent);
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

void
X11_ProcessEvent (void)
{
	XEvent      x_event;

	XNextEvent (x_disp, &x_event);
	if (x_event.type >= LASTEvent) {
		if (x_event.type == x_shmeventtype)
			oktodraw = 1;
		return;
	}
	if (event_handlers[x_event.type])
		event_handlers[x_event.type] (&x_event);
}

void
X11_ProcessEvents (void)
{
	/* Get events from X server. */
	while (XPending (x_disp)) {
		X11_ProcessEvent ();
	}
}

// ========================================================================
// Tragic death handler
// ========================================================================
void
dump_backtrace ()
{
#ifdef HAVE_EXECINFO_H
#define MAXDEPTH 30
	static int count = 0; // don't wanna do this too many times
	void *array[MAXDEPTH];
	size_t size;
	if (sys_backtrace && (count < sys_backtrace->int_val)) {
		count++;
		size = backtrace (array, MAXDEPTH);
		fflush (stderr);
		backtrace_symbols_fd (array, size, STDERR_FILENO);
	}
#endif
}


static void
TragicDeath (int sig)
{
#ifdef HAVE_UNISTD_H
	if (sys_dump_core && sys_dump_core->int_val == 1) { // paranoid check that is NOT needed at time of writing (cvar init happens before rest of init)
		if (fork()) {
			printf ("Received signal %d, dumping core and exiting...\n", sig);
			dump_backtrace ();
			Sys_Quit ();
		} else {
			signal (SIGABRT, SIG_IGN); // is xlib setting a handler on us?
			abort ();
		}
	} else {
#endif
		printf ("Received signal %d, exiting...\n", sig);
		dump_backtrace ();
		Sys_Quit ();
		abort();	// Hopefully not an infinite loop. // never reached
#ifdef HAVE_UNISTD_H
	}
#endif
}

void
X11_OpenDisplay (void)
{
	if (!x_disp) {
		x_disp = XOpenDisplay (NULL);
		if (!x_disp) {
			Sys_Error ("X11_OpenDisplay: Could not open display [%s]\n",
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
//		signal(SIGFPE, TragicDeath);
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
X11_CloseDisplay (void)
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


void X11_ForceMove (int x, int y) {
   int          nx, ny;

   XMoveWindow (x_disp, x_win, x, y);
   XSync (x_disp, false);
   X11_GetWindowCoords (&nx, &ny);
   nx -= x;
   ny -= y;
   if (nx == 0 || ny == 0) {
      return;
   }
   x -= nx;
   y -= ny;

#if 0 // hopefully this isn't needed!  enable if it is.
   if (x < 1 - vid.width)
	   x=0;
   if (y < 1 - vid.height)
	   y=0;
#endif

   XMoveWindow (x_disp, x_win, x, y);
   XSync (x_disp, false);
	 /*this is the best we can do. */
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
	if (!vidmode_avail)
		vidmode_avail = VID_CheckVMode (x_disp, NULL, NULL);

	if (vidmode_avail) {
		vid_gamma_avail = ((x_gamma = X11_GetGamma ()) > 0);
	}

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
			Con_DPrintf ("VID: Chose video mode: %dx%d\n", vid.width, vid.height);

			XF86VidModeSwitchToMode (x_disp, x_screen, vidmodes[best_mode]);
			X11_ForceViewPort ();
			vidmode_active = true;
			X11_SetScreenSaver ();
		} else {
			Con_Printf ("VID: Mode %dx%d can't go fullscreen.\n", vid.width, vid.height);
			vid_gamma_avail = vidmode_avail = vidmode_active = false;
		}
	}
#endif
}

void X11_UpdateFullscreen (cvar_t *fullscreen)
{
	if (!vid_context_created) {
		return;
	}

	if (!fullscreen->int_val) {
		X11_RestoreVidMode ();
		if (in_grab) {
			IN_UpdateGrab(in_grab);
		}
		if (window_saved) {
			X11_ForceMove(window_x, window_y);
			window_saved = 0;
		}
		return;
	} else {
		if (X11_GetWindowCoords (&window_x, &window_y))
			window_saved = 1;
		X11_SetVidMode (scr_width, scr_height);
		if (!vidmode_active) {
			window_saved = 0;
			return;
		}
		X11_ForceMove(0, 0);
		X11_ForceViewPort (); 
		/* Done in X11_SetVidMode but moved the window since then */
		if (in_grab) {
			IN_UpdateGrab(in_grab);
		}
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
	sys_dump_core = Cvar_Get ("sys_dump_core", "0", CVAR_NONE, dump_core_callback, "Dump core on Tragic Death.  Be sure to check 'ulimit -c'");
	sys_backtrace = Cvar_Get ("sys_backtrace", "0", CVAR_NONE, backtrace_callback, "Dump a backtrace on Tragic Death.  Value is the max number of times to dump core incase of recursive shutdown");
}
   
void
X11_CreateWindow (int width, int height)
{
	XSetWindowAttributes attr;
	XClassHint *ClassHint;
	XSizeHints *SizeHints;
	char       *resname;
	unsigned long mask;

	/* window attributes */
	attr.background_pixel = 0;
	attr.colormap = XCreateColormap (x_disp, x_root, x_vis, AllocNone);
	attr.event_mask = X_MASK;
	mask = CWBackPixel | CWColormap | CWEventMask;

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
	X11_SetCaption (va ("%s %s", PROGRAM, VERSION));

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

	XMapRaised (x_disp, x_win);

	while (1) {
		XEvent ev;
		XMaskEvent(x_disp,StructureNotifyMask,&ev);
		if (ev.type==MapNotify) break;
	}
	vid_context_created = true;
	if (vid_fullscreen->int_val) {
		X11_UpdateFullscreen(vid_fullscreen);
	}
}


void
X11_RestoreVidMode (void)
{
#ifdef HAVE_VIDMODE
	if (vidmode_active) {
		X11_RestoreScreenSaver ();
		X11_RestoreGamma ();
		XF86VidModeSwitchToMode (x_disp, x_screen, vidmodes[original_mode]);
		XFree (vidmodes);
		vidmode_active=false;
	}
#endif
}


void
X11_GrabKeyboard (void)
{
	XGrabKeyboard (x_disp, x_win, 1, GrabModeAsync, GrabModeAsync,
				   CurrentTime);
	XSetInputFocus(x_disp,x_win, RevertToPointerRoot,CurrentTime);
}

void
X11_GrabMouse(void)
{
	XGrabPointer (x_disp, x_win, True, MOUSE_MASK, GrabModeAsync,
				  GrabModeAsync, x_win, None, CurrentTime);
}

void
X11_UngrabMouse(void)
{
	XUngrabPointer(x_disp,CurrentTime);
	XWarpPointer(x_disp,x_win,x_win,0,0,0,0,vid.width/2,vid.height/2);
}

void
X11_UngrabKeyboard (void)
{
	XUngrabKeyboard (x_disp, CurrentTime);
}


void
X11_Grabber(qboolean grab)
{
	static qboolean is_grabbed=false;

	if (!vid_context_created) {
		Con_Printf("No video context to grab to!\n");
		return;
	}
	if (grab) {
		if (!is_grabbed) {
			X11_GrabKeyboard();
			X11_GrabMouse();
			is_grabbed=true;
		}
	} else {
		if (is_grabbed) {
			X11_UngrabKeyboard();
			X11_UngrabMouse();
			is_grabbed=false;
		}
	}
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
	Window      theroot, scrap;
	int         x, y;
	unsigned int width, height, bdwidth, depth;

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
	int         ax, ay;

	if (!vidmode_avail  || !vid_context_created)
		return;

	if (!X11_GetWindowCoords (&ax, &ay)) {
		/* "icky kludge code" */
		Con_Printf ("VID: Falling back on warp kludge to set viewport.\n");
		XWarpPointer (x_disp, None, x_win, 0, 0, 0, 0, scr_width, scr_height);
		XWarpPointer (x_disp, None, x_win, 0, 0, 0, 0, 0, 0);
	}
	XF86VidModeSetViewPort (x_disp, x_screen, ax, ay);
}

double
X11_GetGamma (void)
{
#ifdef HAVE_VIDMODE
# ifdef X_XF86VidModeGetGamma
	XF86VidModeGamma	xgamma;
	
	if (vidmode_avail && vid_system_gamma->int_val) {
		if (XF86VidModeGetGamma (x_disp, x_screen, &xgamma)) {
			return ((xgamma.red + xgamma.green + xgamma.blue) / 3);
		}
	}
# endif
#endif
	return -1.0;
}

qboolean
X11_SetGamma (double gamma)
{
#ifdef HAVE_VIDMODE
# ifdef X_XF86VidModeSetGamma
	XF86VidModeGamma	xgamma;
	
	if (vid_gamma_avail && vid_system_gamma->int_val) {
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

	if (vid_gamma_avail) {
		xgamma.red = xgamma.green = xgamma.blue = (float) x_gamma;
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
