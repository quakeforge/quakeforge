/*
	vid_x11_gl.c

	GLX X11 video driver

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999-2000  contributors of the QuakeForge project
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
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
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <dlfcn.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "QF/cvar.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "context_x11.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_gl.h"

#define GLX_RGBA				4		// true if RGBA mode
#define GLX_DOUBLEBUFFER		5		// double buffering supported
#define GLX_RED_SIZE			8		// number of red component bits
#define GLX_GREEN_SIZE			9		// number of green component bits
#define GLX_BLUE_SIZE			10		// number of blue component bits
#define GLX_DEPTH_SIZE			12		// number of depth bits

typedef XID GLXDrawable;

// Define GLAPIENTRY to a useful value
#ifndef GLAPIENTRY
# ifdef _WIN32
#  include <windows.h>
#  define GLAPIENTRY WINAPI
#  undef LoadImage
# else
#  ifdef APIENTRY
#   define GLAPIENTRY APIENTRY
#  else
#   define GLAPIENTRY
#  endif
# endif
#endif
static void    *libgl_handle;
static void (*qfglXSwapBuffers) (Display *dpy, GLXDrawable drawable);
static XVisualInfo* (*qfglXChooseVisual) (Display *dpy, int screen,
										  int *attribList);
static GLXContext (*qfglXCreateContext) (Display *dpy, XVisualInfo *vis,
										 GLXContext shareList, Bool direct);
static Bool (*qfglXMakeCurrent) (Display *dpy, GLXDrawable drawable,
								 GLXContext ctx);
static void (GLAPIENTRY *qfglFinish) (void);
static void *(*glGetProcAddress) (const char *symbol) = NULL;
static int use_gl_procaddress = 0;

static cvar_t  *gl_driver;

static void *
QFGL_GetProcAddress (void *handle, const char *name)
{
	void       *glfunc = NULL;

	if (use_gl_procaddress && glGetProcAddress)
		glfunc = glGetProcAddress (name);
	if (!glfunc)
		glfunc = dlsym (handle, name);
	return glfunc;
}

static void *
QFGL_ProcAddress (const char *name, qboolean crit)
{
	void    *glfunc = NULL;

	Sys_MaskPrintf (SYS_VID, "DEBUG: Finding symbol %s ... ", name);

	glfunc = QFGL_GetProcAddress (libgl_handle, name);
	if (glfunc) {
		Sys_MaskPrintf (SYS_VID, "found [%p]\n", glfunc);
		return glfunc;
	}
	Sys_MaskPrintf (SYS_VID, "not found\n");

	if (crit) {
		if (strncmp ("fxMesa", name, 6) == 0) {
			Sys_Printf ("This target requires a special version of Mesa with "
						"support for Glide and SVGAlib.\n");
			Sys_Printf ("If you are in X, try using a GLX or SGL target.\n");
		}
		Sys_Error ("Couldn't load critical OpenGL function %s, exiting...",
				   name);
	}
	return NULL;
}

static void
glx_choose_visual (gl_ctx_t *ctx)
{
	int         attrib[] = {
		GLX_RGBA,
		GLX_RED_SIZE, 1,
		GLX_GREEN_SIZE, 1,
		GLX_BLUE_SIZE, 1,
		GLX_DOUBLEBUFFER,
		GLX_DEPTH_SIZE, 1,
		None
	};

	x_visinfo = qfglXChooseVisual (x_disp, x_screen, attrib);
	if (!x_visinfo) {
		Sys_Error ("Error couldn't get an RGB, Double-buffered, Depth visual");
	}
	x_vis = x_visinfo->visual;
}

static void
glx_create_context (gl_ctx_t *ctx)
{
	XSync (x_disp, 0);
	ctx->context = qfglXCreateContext (x_disp, x_visinfo, NULL, True);
	qfglXMakeCurrent (x_disp, x_win, ctx->context);
	ctx->init_gl ();
}

static void
glx_end_rendering (void)
{
	qfglFinish ();
	qfglXSwapBuffers (x_disp, x_win);
}

static void
glx_load_gl (void)
{
	libgl_handle = dlopen (gl_driver->string, RTLD_NOW);
	if (!libgl_handle) {
		Sys_Error ("Couldn't load OpenGL library %s: %s", gl_driver->string,
				   dlerror ());
	}
	glGetProcAddress = dlsym (libgl_handle, "glXGetProcAddress");
	if (!glGetProcAddress)
		glGetProcAddress = dlsym (libgl_handle, "glXGetProcAddressARB");

	qfglXSwapBuffers = QFGL_ProcAddress ("glXSwapBuffers", true);
	qfglXChooseVisual = QFGL_ProcAddress ("glXChooseVisual", true);
	qfglXCreateContext = QFGL_ProcAddress ("glXCreateContext", true);
	qfglXMakeCurrent = QFGL_ProcAddress ("glXMakeCurrent", true);

	use_gl_procaddress = 1;

	qfglFinish = QFGL_ProcAddress ("glFinish", true);
}

gl_ctx_t *
X11_GL_Context (void)
{
	gl_ctx_t *ctx = calloc (1, sizeof (gl_ctx_t));
	ctx->load_gl = glx_load_gl;
	ctx->choose_visual = glx_choose_visual;
	ctx->create_context = glx_create_context;
	ctx->get_proc_address = QFGL_ProcAddress;
	ctx->end_rendering = glx_end_rendering;
	return ctx;
}

void
X11_GL_Init_Cvars (void)
{
	gl_driver = Cvar_Get ("gl_driver", GL_DRIVER, CVAR_ROM, NULL,
						  "The OpenGL library to use. (path optional)");
}
