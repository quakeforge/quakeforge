/*
	vid_wl_gl.c

	OpenGL Wayland video driver

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

#include "QF/cvar.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "context_wl.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_gl.h"

#include <wayland-egl.h>

static void    *libgl_handle;

// FIXME: CVar (check X11 / Windows impl)
static const char *gl_driver = "libEGL.so.1";

typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef int EGLint;
typedef void *EGLDisplay;
typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLSurface;

typedef struct wl_egl_window *EGLNativeWindowType;

#define EGL_NONE			0x3038
#define EGL_FALSE			0
#define EGL_TRUE			1

#define EGL_NO_DISPLAY 		(EGLDisplay) nullptr
#define EGL_NO_CONFIG 		(EGLConfig) nullptr
#define EGL_NO_CONTEXT 		(EGLContext) nullptr
#define EGL_NO_SURFACE 		(EGLSurface) nullptr

#define EGL_SURFACE_TYPE		0x3033
#define EGL_TRANSPARENT_TYPE	0x3034
#define EGL_WINDOW_BIT			0x0004

#define EGL_EXTENSIONS		0x3055

#define EGL_RED_SIZE		0x3024
#define EGL_GREEN_SIZE		0x3023
#define EGL_BLUE_SIZE		0x3022
#define EGL_ALPHA_SIZE		0x3021
#define EGL_DEPTH_SIZE		0x3025

#define EGL_RENDERABLE_TYPE	0x3040
#define EGL_OPENGL_BIT		0x0008
#define EGL_OPENGL_API		0x30A2

#define EGL_PLATFORM_WAYLAND_EXT		0x31D8

#define EGL_CONTEXT_MAJOR_VERSION		0x3098
#define EGL_CONTEXT_MINOR_VERSION		0x30FB
#define EGL_CONTEXT_OPENGL_PROFILE_MASK	0x30FD

#define EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT				0x00000001
#define EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT	0x00000002

static void *(*eglGetProcAddress) (const char *symbol);
static EGLint (*eglGetError) (void);
static EGLDisplay (*eglGetPlatformDisplay) (EGLenum platform,
											void *native_display,
											const EGLint *attrib_list);
static const char *(*eglQueryString) (EGLDisplay, EGLint);
static EGLBoolean (*eglInitialize) (EGLDisplay dpy,
									EGLint *major, EGLint *minor);
static EGLBoolean (*eglGetConfigs) (EGLDisplay dpy, EGLConfig *configs,
									EGLint config_size, EGLint *num_config);
static EGLBoolean (*eglChooseConfig) (EGLDisplay dpy, const EGLint *attrib_list,
									  EGLConfig *configs, EGLint config_size,
									  EGLint *num_config);
static EGLBoolean (*eglGetConfigAttrib) (EGLDisplay dpy, EGLConfig config,
										 EGLint attribute, EGLint *value);
static EGLBoolean (*eglBindAPI) (EGLenum api);
static EGLSurface (*eglCreatePlatformWindowSurface) (EGLDisplay dpy,
													 EGLConfig config,
													 void *win,
													 const EGLint *attrib_list);
static EGLContext (*eglCreateContext) (EGLDisplay dpy, EGLConfig config,
									   EGLContext share_context,
									   const EGLint *attrib_list);
static EGLBoolean (*eglMakeCurrent) (EGLDisplay dpy, EGLSurface draw,
									 EGLSurface read, EGLContext ctx);
static EGLBoolean (*eglSwapBuffers) (EGLDisplay, EGLSurface);
static bool use_egl_procaddress = false;

static void (*qfglFinish) (void);

static EGLDisplay egl_display;
static EGLConfig egl_config;
static EGLContext egl_context;
static EGLSurface egl_surface;

static struct wl_egl_window *egl_window;

static void *
QFGL_GetProcAddress (void *handle, const char *name)
{
	void       *glfunc = NULL;
	if (use_egl_procaddress && eglGetProcAddress)
		glfunc = eglGetProcAddress (name);
	if (!glfunc)
		glfunc = dlsym (handle, name);
	return glfunc;
}

static void
egl_choose_visual (gl_ctx_t *ctx)
{
	egl_display = eglGetPlatformDisplay (EGL_PLATFORM_WAYLAND_EXT, wl_display,
										 nullptr);
	if (egl_display == EGL_NO_DISPLAY) {
		Sys_Error ("Failed to get EGL display: %x", eglGetError ());
	}

	EGLint egl_major, egl_minor;
	if (eglInitialize (egl_display, &egl_major, &egl_minor) != EGL_TRUE) {
		Sys_Error ("Failed to initialize EGL, error: %x", eglGetError ());
	}
	Sys_Printf ("Initialized EGL %d.%d\n", egl_major, egl_minor);

	EGLint config_attribs[] = {
		EGL_SURFACE_TYPE,		EGL_WINDOW_BIT,
		EGL_RED_SIZE,			8,
		EGL_GREEN_SIZE,			8,
		EGL_BLUE_SIZE,			8,
		//EGL_ALPHA_SIZE,		8, /* FIXME: Enabling this causes window to go transparent */
		EGL_DEPTH_SIZE,			24,
		EGL_RENDERABLE_TYPE,	EGL_OPENGL_BIT,

		EGL_NONE
	};

	EGLint num_configs = 0;
	eglGetConfigs (egl_display, nullptr, 0, &num_configs);
	Sys_Printf ("Found %d EGL configs\n", num_configs);
	EGLConfig *configs = calloc (num_configs, sizeof (EGLConfig));
	EGLint num_valid_configs = 0;
	eglChooseConfig (egl_display, config_attribs,
					 configs, num_configs, &num_valid_configs);
	for (EGLint i = 0; i < num_valid_configs; ++i) {
		EGLint renderable_type = 0;
		eglGetConfigAttrib (egl_display, configs[i],
							EGL_RENDERABLE_TYPE, &renderable_type);

		if (renderable_type & EGL_OPENGL_BIT) {
			egl_config = configs[i];
			break;
		}
	}
	if (egl_config == EGL_NO_CONFIG) {
		Sys_Error ("Failed to find appropriate EGL config");
	}

}

static void
vidsize_listener (void *data, const viddef_t *vid)
{
	wl_egl_window_resize (egl_window, vid->width, vid->height, 0, 0);
}

static void
egl_create_context (gl_ctx_t *ctx, int core)
{
	eglBindAPI (EGL_OPENGL_API);

	egl_window = wl_egl_window_create (wl_surface, viddef.width, viddef.height);
	egl_surface = eglCreatePlatformWindowSurface (egl_display, egl_config,
												  egl_window, nullptr);
	if (egl_surface == EGL_NO_SURFACE) {
		Sys_Error ("Failed to create EGL window surface: %x", eglGetError ());
	}
	Sys_Printf ("Created EGL window surface\n");

	EGLint context_attribs[] = {
		EGL_CONTEXT_MAJOR_VERSION, 3,
		EGL_CONTEXT_MINOR_VERSION, 0,
		EGL_CONTEXT_OPENGL_PROFILE_MASK,
			core ? EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT
				 : EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT,

		EGL_NONE
	};
	egl_context = eglCreateContext (egl_display, egl_config, EGL_NO_CONTEXT,
									context_attribs);
	if (egl_context == EGL_NO_CONTEXT) {
		Sys_Error ("Failed to create EGL context: %x", eglGetError ());
	}
	Sys_Printf ("Created EGL context successfully\n");

	ctx->context = (GL_context) egl_context;

	eglMakeCurrent (egl_display, egl_surface, egl_surface, egl_context);

	VID_OnVidResize_AddListener (vidsize_listener, ctx);

	ctx->init_gl ();
}

static void *
egl_get_procaddress (const char *name, bool crit)
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
egl_end_rendering (void)
{
	qfglFinish ();
	eglSwapBuffers (egl_display, egl_surface);
}

static void
egl_load_gl (gl_ctx_t *ctx)
{
	libgl_handle = dlopen (gl_driver, RTLD_NOW);
	if (!libgl_handle) {
		Sys_Error ("Couldn't load OpenGL library %s: %s", gl_driver,
				   dlerror ());
	}

	eglGetProcAddress = QFGL_GetProcAddress (libgl_handle, "eglGetProcAddress");
	eglGetError = QFGL_GetProcAddress (libgl_handle, "eglGetError");
	eglQueryString = QFGL_GetProcAddress (libgl_handle, "eglQueryString");
	eglGetPlatformDisplay = QFGL_GetProcAddress (libgl_handle, "eglGetPlatformDisplay");
	eglInitialize = QFGL_GetProcAddress (libgl_handle, "eglInitialize");
	eglGetConfigs = QFGL_GetProcAddress (libgl_handle, "eglGetConfigs");
	eglChooseConfig = QFGL_GetProcAddress (libgl_handle, "eglChooseConfig");
	eglGetConfigAttrib = QFGL_GetProcAddress (libgl_handle, "eglGetConfigAttrib");
	eglBindAPI = QFGL_GetProcAddress (libgl_handle, "eglBindAPI");
	eglCreatePlatformWindowSurface = QFGL_GetProcAddress (libgl_handle, "eglCreateWindowSurface");
	eglCreateContext = QFGL_GetProcAddress (libgl_handle, "eglCreateContext");
	eglMakeCurrent = QFGL_GetProcAddress (libgl_handle, "eglMakeCurrent");
	eglSwapBuffers = QFGL_GetProcAddress (libgl_handle, "eglSwapBuffers");

	use_egl_procaddress = true;

	qfglFinish = egl_get_procaddress ("glFinish", true);
}

static void
egl_unload_gl (void *_ctx)
{
	gl_ctx_t   *ctx = _ctx;
	if (libgl_handle) {
		dlclose (libgl_handle);
		libgl_handle = 0;
	}
	free (ctx);
}

gl_ctx_t *WL_GL_Context (vid_internal_t *vi) __attribute__((const));
gl_ctx_t *
WL_GL_Context (vid_internal_t *vi)
{
	gl_ctx_t *ctx = calloc (1, sizeof (gl_ctx_t));
	ctx->load_gl = egl_load_gl;
	ctx->choose_visual = egl_choose_visual;
	ctx->create_context = egl_create_context;
	ctx->get_proc_address = egl_get_procaddress;
	ctx->end_rendering = egl_end_rendering;

	vi->unload = egl_unload_gl;
	vi->ctx = ctx;
	return ctx;
}

void
WL_GL_Init_Cvars (void)
{
}
