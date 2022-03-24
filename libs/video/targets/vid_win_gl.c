/*
	vid_win_gl.c

	Win32 GL vid component

	Copyright (C) 1996-1997  Id Software, Inc.

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

#include "winquake.h"
#include <ddraw.h>

#include "QF/cvar.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "context_win.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_gl.h"

// Define GLAPIENTRY to a useful value
#ifndef GLAPIENTRY
# define GLAPIENTRY WINAPI
#endif
static void *libgl_handle;
static HGLRC (GLAPIENTRY * qfwglCreateContext) (HDC);
static BOOL (GLAPIENTRY * qfwglDeleteContext) (HGLRC);
static HGLRC (GLAPIENTRY * qfwglGetCurrentContext) (void);
static HDC (GLAPIENTRY * qfwglGetCurrentDC) (void);
static BOOL (GLAPIENTRY * qfwglMakeCurrent) (HDC, HGLRC);
static void (GLAPIENTRY *qfglFinish) (void);
static void *(WINAPI * glGetProcAddress) (const char *symbol) = NULL;
static int use_gl_proceaddress = 0;

static cvar_t *gl_driver;
static HGLRC   baseRC;//FIXME should be in gl_ctx_t, but that's GLXContext...
static void *
QFGL_GetProcAddress (void *handle, const char *name)
{
	void       *glfunc = NULL;

	if (use_gl_proceaddress && glGetProcAddress)
		glfunc = glGetProcAddress (name);
	if (!glfunc)
		glfunc = GetProcAddress (handle, name);
	return glfunc;
}

static void *
QFGL_ProcAddress (const char *name, qboolean crit)
{
	void       *glfunc = NULL;

	Sys_MaskPrintf (SYS_vid, "DEBUG: Finding symbol %s ... ", name);

	glfunc = QFGL_GetProcAddress (libgl_handle, name);
	if (glfunc) {
		Sys_MaskPrintf (SYS_vid, "found [%p]\n", glfunc);
		return glfunc;
	}
	Sys_MaskPrintf (SYS_vid, "not found\n");

	if (crit) {
		Sys_Error ("Couldn't load critical OpenGL function %s, exiting...",
				   name);
	}
	return NULL;
}

static void
wgl_choose_visual (gl_ctx_t *ctx)
{
}

static void
wgl_set_pixel_format (void)
{
	int         pixelformat;
	PIXELFORMATDESCRIPTOR pfd = {
		.nSize = sizeof(PIXELFORMATDESCRIPTOR),
		.nVersion = 1,
		.dwFlags = PFD_DOUBLEBUFFER | PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW,
		.iPixelType = PFD_TYPE_RGBA,
		.cColorBits = win_gdevmode.dmBitsPerPel,
		.cDepthBits = 32,
		.iLayerType = PFD_MAIN_PLANE,
	};

	if (!(pixelformat = ChoosePixelFormat (win_maindc, &pfd))) {
		Sys_Error ("ChoosePixelFormat failed");
	}
	if (!SetPixelFormat (win_maindc, pixelformat, &pfd)) {
		Sys_Error ("SetPixelFormat failed");
	}
}

static void
wgl_create_context (gl_ctx_t *ctx, int core)
{
	DWORD       lasterror;

	win_maindc = GetDC (win_mainwindow);

	wgl_set_pixel_format ();
	baseRC = qfwglCreateContext (win_maindc);
	if (!baseRC) {
		lasterror=GetLastError();
		if (win_maindc && win_mainwindow)
			ReleaseDC (win_mainwindow, win_maindc);
		Sys_Error ("Could not initialize GL (wglCreateContext failed).\n\n"
				   "Make sure you are in 65535 color mode, and try running "
				   "with -window.\n"
				   "Error code: (%lx)", lasterror);
	}

	if (!qfwglMakeCurrent (win_maindc, baseRC)) {
		lasterror = GetLastError ();
		if (baseRC)
			qfwglDeleteContext (baseRC);
		if (win_maindc && win_mainwindow)
			ReleaseDC (win_mainwindow, win_maindc);
		Sys_Error ("wglMakeCurrent failed (%lx)", lasterror);
	}

	ctx->init_gl ();
}

static void
wgl_end_rendering (void)
{
	if (!scr_skipupdate) {
		qfglFinish ();
		SwapBuffers (win_maindc);
	}
	// handle the mouse state when windowed if that's changed
	if (!vid_fullscreen->int_val) {
//FIXME		if (!in_grab->int_val) {
//FIXME         if (windowed_mouse) {
//FIXME             IN_DeactivateMouse ();
//FIXME             IN_ShowMouse ();
//FIXME             windowed_mouse = false;
//FIXME         }
//FIXME		} else {
//FIXME         windowed_mouse = true;
//FIXME		}
	}
}

static void
wgl_load_gl (void)
{
	libgl_handle = LoadLibrary (gl_driver->string);
	if (!libgl_handle) {
		Sys_Error ("Couldn't load OpenGL library %s!", gl_driver->string);
	}
	glGetProcAddress =
		(void *) GetProcAddress (libgl_handle, "wglGetProcAddress");

	qfwglCreateContext = QFGL_ProcAddress ("wglCreateContext", true);
	qfwglDeleteContext = QFGL_ProcAddress ("wglDeleteContext", true);
	qfwglGetCurrentContext = QFGL_ProcAddress ("wglGetCurrentContext", true);
	qfwglGetCurrentDC = QFGL_ProcAddress ("wglGetCurrentDC", true);
	qfwglMakeCurrent = QFGL_ProcAddress ("wglMakeCurrent", true);

	use_gl_proceaddress = 1;

	qfglFinish = QFGL_ProcAddress ("glFinish", true);
}

gl_ctx_t *
Win_GL_Context (void)
{
	gl_ctx_t *ctx = calloc (1, sizeof (gl_ctx_t));
	ctx->load_gl = wgl_load_gl;
	ctx->choose_visual = wgl_choose_visual;
	ctx->create_context = wgl_create_context;
	ctx->get_proc_address = QFGL_ProcAddress;
	ctx->end_rendering = wgl_end_rendering;
	return ctx;
}

void
Win_GL_Init_Cvars (void)
{
	gl_driver = Cvar_Get ("gl_driver", GL_DRIVER, CVAR_ROM, NULL,
						  "The OpenGL library to use. (path optional)");
}
