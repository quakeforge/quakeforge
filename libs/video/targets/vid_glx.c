/*
	vid_glx.c

	OpenGL GLX video driver

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]

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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_DLOPEN
# include <dlfcn.h>
#endif

#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#ifdef HAVE_DGA
# include <X11/extensions/xf86dga.h>
#endif

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/input.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"
#include "QF/GL/funcs.h"

#include "compat.h"
#include "context_x11.h"
#include "sbar.h"
#include "r_cvar.h"

#define WARP_WIDTH		320
#define WARP_HEIGHT 	200

/* GLXContext is a pointer to opaque data. */
typedef struct __GLXcontextRec *GLXContext;

#define GLX_RGBA                4       /* true if RGBA mode */
#define GLX_DOUBLEBUFFER        5       /* double buffering supported */
#define GLX_RED_SIZE            8       /* number of red component bits */
#define GLX_GREEN_SIZE          9       /* number of green component bits */
#define GLX_BLUE_SIZE           10      /* number of blue component bits */
#define GLX_DEPTH_SIZE          12      /* number of depth bits */

static GLXContext ctx = NULL;
typedef XID GLXDrawable;

void (* glXSwapBuffers) (Display *dpy, GLXDrawable drawable);
XVisualInfo* (* glXChooseVisual) (Display *dpy, int screen, int *attribList);
GLXContext (* glXCreateContext) (Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct);
Bool (* glXMakeCurrent) (Display *dpy, GLXDrawable drawable, GLXContext ctx);

extern void GL_Init_Common (void);
extern void VID_Init8bitPalette (void);

/*-----------------------------------------------------------------------*/

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;
void	   *libgl_handle;


void
VID_Shutdown (void)
{
	if (!vid.initialized)
		return;

	Con_Printf ("VID_Shutdown\n");

	X11_RestoreVidMode ();
	X11_CloseDisplay ();
}

void
GL_Init (void)
{
	GL_Init_Common ();
}

void
GL_EndRendering (void)
{
	qfglFlush ();
	glXSwapBuffers (x_disp, x_win);
	Sbar_Changed ();
}

void
VID_Center_f (void) {
	X11_ForceViewPort ();
}

void
VID_Init (unsigned char *palette)
{
	int         i;
	int         attrib[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		None
	};

#ifdef HAVE_DLOPEN
	if (!(libgl_handle = dlopen (gl_driver->string, RTLD_NOW))) {
		Sys_Error ("Can't open OpenGL library \"%s\": %s\n", gl_driver->string,
				   dlerror());
		return;
	}
#else
# error "No dynamic library support. FIXME."
#endif

	glXSwapBuffers = QFGL_ProcAddress (libgl_handle, "glXSwapBuffers", true);
	glXChooseVisual = QFGL_ProcAddress (libgl_handle, "glXChooseVisual", true);
	glXCreateContext = QFGL_ProcAddress (libgl_handle, "glXCreateContext",
										 true);
	glXMakeCurrent = QFGL_ProcAddress (libgl_handle, "glXMakeCurrent", true);
	QFGL_ProcAddress (NULL, NULL, false);	// make ProcAddress clear its cache

	Cmd_AddCommand ("vid_center", VID_Center_f, "Center the view port on the "
					"quake window in a virtual desktop.\n");

	VID_GetWindowSize (640, 480);
	Con_CheckResize (); // Now that we have a window size, fix console

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap8 = vid_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap8 + 2048));

	/* Interpret command-line params */

	/* Set vid parameters */

	if ((i = COM_CheckParm ("-conwidth")))
		vid.conwidth = atoi (com_argv[i + 1]);
	else
		vid.conwidth = scr_width;

	vid.conwidth &= 0xfff8;				// make it a multiple of eight
	vid.conwidth = max (vid.conwidth, 320);

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	if ((i = COM_CheckParm ("-conheight")))	// conheight no smaller than
											// 200px
		vid.conheight = atoi (com_argv[i + 1]);
	vid.conheight = max (vid.conheight, 200);

	X11_OpenDisplay ();

	x_visinfo = glXChooseVisual (x_disp, x_screen, attrib);
	if (!x_visinfo) {
		Sys_Error ("Error couldn't get an RGB, Double-buffered, Depth "
				   "visual\n");
	}
	x_vis = x_visinfo->visual;

	X11_SetVidMode (scr_width, scr_height);
	X11_CreateWindow (scr_width, scr_height);
	/* Invisible cursor */
	X11_CreateNullCursor ();

	XSync (x_disp, 0);

	ctx = glXCreateContext (x_disp, x_visinfo, NULL, True);

	glXMakeCurrent (x_disp, x_win, ctx);

	vid.height = vid.conheight = min (vid.conheight, scr_height);
	vid.width = vid.conwidth = min (vid.conwidth, scr_width);

	vid.aspect = ((float) vid.height / (float) vid.width) * (320.0 / 240.0);
	vid.numpages = 2;

	// InitSig (); // trap evil signals

	GL_Init ();

	VID_InitGamma (palette);

	// Check for 8-bit extension and initialize if present
	VID_Init8bitPalette ();
	VID_SetPalette (palette);

	Con_Printf ("Video mode %dx%d initialized.\n", scr_width, scr_height);

	vid.initialized = true;

	vid.recalc_refdef = 1;				// force a surface cache flush
}

void
VID_Init_Cvars ()
{
	X11_Init_Cvars ();
}

void
VID_SetCaption (const char *text)
{
	if (text && *text) {
		char	*temp = strdup (text);

		X11_SetCaption (va ("%s %s: %s", PROGRAM, VERSION, temp));
		free (temp);
	} else {
		X11_SetCaption (va ("%s %s", PROGRAM, VERSION));
	}
}

double
VID_GetGamma (void)
{
	return (double) X11_GetGamma ();
}

void
VID_ForceViewPort(void)
{
	X11_ForceViewPort ();
}

qboolean
VID_SetGamma (double gamma)
{
	return X11_SetGamma (gamma);
}
