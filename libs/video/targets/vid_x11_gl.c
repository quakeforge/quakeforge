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
#define GLX_ALPHA_SIZE			11		// number of alpha component bits
#define GLX_DEPTH_SIZE			12		// number of depth bits
#define GLX_STENCIL_SIZE		13		// number of stencil bits
#define GLX_CONTEXT_MAJOR_VERSION_ARB   0x2091
#define GLX_CONTEXT_MINOR_VERSION_ARB   0x2092
#define GLX_CONTEXT_FLAGS_ARB           0x2094
#define GLX_CONTEXT_PROFILE_MASK_ARB    0x9126
#define GLX_CONTEXT_DEBUG_BIT_ARB               0x0001
#define GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB  0x0002
#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB            0x00000001
#define GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB   0x00000002

#define GLX_X_RENDERABLE  0x8012
#define GLX_DRAWABLE_TYPE 0x8010
#define GLX_WINDOW_BIT    0x00000001
#define GLX_RENDER_TYPE   0x8011
#define GLX_RGBA_BIT      0x00000001
#define GLX_X_VISUAL_TYPE 0x22
#define GLX_TRUE_COLOR    0x8002


typedef XID GLXDrawable;

// GLXContext is a pointer to opaque data
typedef struct __GLXcontextRec *GLXContext;
typedef struct __GLXFBConfigRec *GLXFBConfig;

// Define GLAPIENTRY to a useful value
#ifndef GLAPIENTRY
#  ifdef APIENTRY
#   define GLAPIENTRY APIENTRY
#  else
#   define GLAPIENTRY
#  endif
#endif
static void    *libgl_handle;
static void (*qfglXSwapBuffers) (Display *dpy, GLXDrawable drawable);
static GLXContext (*qfglXCreateContextAttribsARB) (Display *dpy,
												   GLXFBConfig cfg,
												   GLXContext ctx,
												   Bool direct,
												   const int *attribs);
static GLXFBConfig *(*qfglXChooseFBConfig) (Display *dpy, int screen,
											const int *attribs, int *nitems);
static XVisualInfo *(*qfglXGetVisualFromFBConfig) (Display *dpy,
												   GLXFBConfig config);
static int (*qfglXGetFBConfigAttrib) (Display *dpy, GLXFBConfig config,
									  int attribute, int *value);
static Bool (*qfglXMakeCurrent) (Display *dpy, GLXDrawable drawable,
								 GLXContext ctx);
static void (GLAPIENTRY *qfglFinish) (void);
static void *(*glGetProcAddress) (const char *symbol) = NULL;
static int use_gl_procaddress = 0;

static GLXFBConfig glx_cfg;
static char *gl_driver;
static cvar_t gl_driver_cvar = {
	.name = "gl_driver",
	.description =
		"The OpenGL library to use. (path optional)",
	.default_value = GL_DRIVER,
	.flags = CVAR_ROM,
	.value = { .type = 0, .value = &gl_driver },
};

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

	Sys_MaskPrintf (SYS_vid, "DEBUG: Finding symbol %s ... ", name);

	glfunc = QFGL_GetProcAddress (libgl_handle, name);
	if (glfunc) {
		Sys_MaskPrintf (SYS_vid, "found [%p]\n", glfunc);
		return glfunc;
	}
	Sys_MaskPrintf (SYS_vid, "not found\n");

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
		GLX_X_RENDERABLE,     True,
		GLX_DRAWABLE_TYPE,    GLX_WINDOW_BIT,
		GLX_RENDER_TYPE,      GLX_RGBA_BIT,
		GLX_X_VISUAL_TYPE,    GLX_TRUE_COLOR,
		GLX_RED_SIZE,         8,
		GLX_GREEN_SIZE,       8,
		GLX_BLUE_SIZE,        8,
		GLX_ALPHA_SIZE,       8,
		GLX_DEPTH_SIZE,       24,
		//I can't tell if this makes any difference to performance, but it's
		//currently not needed
		//GLX_STENCIL_SIZE,     8,
		GLX_DOUBLEBUFFER,     True,
		None
	};
	int         fbcount;
	GLXFBConfig *fbc = qfglXChooseFBConfig (x_disp, x_screen, attrib, &fbcount);

	if (!fbc) {
		Sys_Error ("Failed to retrieve a framebuffer config");
	}
	Sys_MaskPrintf (SYS_vid, "Found %d matching FB configs.\n", fbcount);
	glx_cfg = fbc[0];
	XFree (fbc);
	x_visinfo = qfglXGetVisualFromFBConfig (x_disp, glx_cfg);
	if (!x_visinfo) {
		Sys_Error ("Error couldn't get an RGB, Double-buffered, Depth visual");
	}
	x_vis = x_visinfo->visual;
}

static void
glx_create_context (gl_ctx_t *ctx, int core)
{
	int         attribs[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
		GLX_CONTEXT_MINOR_VERSION_ARB, 0,
		GLX_CONTEXT_PROFILE_MASK_ARB,
			core ? GLX_CONTEXT_CORE_PROFILE_BIT_ARB
				 : GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
		None
	};
	XSync (x_disp, 0);
	ctx->context = (GL_context) qfglXCreateContextAttribsARB (x_disp, glx_cfg,
													0, True, attribs);
	qfglXMakeCurrent (x_disp, x_win, (GLXContext) ctx->context);
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
	libgl_handle = dlopen (gl_driver, RTLD_NOW);
	if (!libgl_handle) {
		Sys_Error ("Couldn't load OpenGL library %s: %s", gl_driver,
				   dlerror ());
	}
	glGetProcAddress = dlsym (libgl_handle, "glXGetProcAddress");
	if (!glGetProcAddress)
		glGetProcAddress = dlsym (libgl_handle, "glXGetProcAddressARB");

	qfglXSwapBuffers = QFGL_ProcAddress ("glXSwapBuffers", true);
	qfglXChooseFBConfig = QFGL_ProcAddress ("glXChooseFBConfig", true);
	qfglXGetVisualFromFBConfig = QFGL_ProcAddress ("glXGetVisualFromFBConfig", true);
	qfglXGetFBConfigAttrib = QFGL_ProcAddress ("glXGetFBConfigAttrib", true);
	qfglXCreateContextAttribsARB = QFGL_ProcAddress ("glXCreateContextAttribsARB", true);
	qfglXMakeCurrent = QFGL_ProcAddress ("glXMakeCurrent", true);

	use_gl_procaddress = 1;

	qfglFinish = QFGL_ProcAddress ("glFinish", true);
}

static void
glx_unload_gl (gl_ctx_t *ctx)
{
	dlclose (libgl_handle);
	free (ctx);
}

gl_ctx_t *
X11_GL_Context (void)
{
	gl_ctx_t *ctx = calloc (1, sizeof (gl_ctx_t));
	ctx->load_gl = glx_load_gl;
	ctx->unload_gl = glx_unload_gl;
	ctx->choose_visual = glx_choose_visual;
	ctx->create_context = glx_create_context;
	ctx->get_proc_address = QFGL_ProcAddress;
	ctx->end_rendering = glx_end_rendering;
	return ctx;
}

void
X11_GL_Init_Cvars (void)
{
	Cvar_Register (&gl_driver_cvar, 0, 0);
}
