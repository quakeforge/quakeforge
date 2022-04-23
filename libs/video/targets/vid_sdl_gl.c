/*
	vid_sdl.c

	Video driver for Sam Lantinga's Simple DirectMedia Layer

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdlib.h>
#include <SDL.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/qendian.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "context_sdl.h"
#include "d_iface.h"
#include "vid_internal.h"
#include "vid_gl.h"

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

static void (GLAPIENTRY *qfglFinish) (void);
static int use_gl_procaddress = 0;
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
QFGL_ProcAddress (const char *name, qboolean crit)
{
	void    *glfunc = NULL;

	Sys_MaskPrintf (SYS_vid, "DEBUG: Finding symbol %s ... ", name);

	glfunc = SDL_GL_GetProcAddress (name);
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
sdlgl_set_vid_mode (gl_ctx_t *ctx)
{
	int         i, j;

	sdl_flags |= SDL_OPENGL;

	// Setup GL Attributes
	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
//	SDL_GL_SetAttribute (SDL_GL_STENCIL_SIZE, 0);	// Try for 0, 8
//	SDL_GL_SetAttribute (SDL_GL_STEREO, 1);			// Someday...

	for (i = 0; i < 5; i++) {
		int k;
		int color[5] = {32, 24, 16, 15, 0};
		int rgba[5][4] = {
			{8, 8, 8, 0},
			{8, 8, 8, 8},
			{5, 6, 5, 0},
			{5, 5, 5, 0},
			{5, 5, 5, 1},
		};

		SDL_GL_SetAttribute (SDL_GL_RED_SIZE, rgba[i][0]);
		SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, rgba[i][1]);
		SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, rgba[i][2]);
		SDL_GL_SetAttribute (SDL_GL_ALPHA_SIZE, rgba[i][3]);

		for (j = 0; j < 5; j++) {
			for (k = 32; k >= 16; k -= 8) {
				SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, k);
				if ((sdl_screen = SDL_SetVideoMode (viddef.width, viddef.height,
													color[j], sdl_flags)))
					goto success;
			}
		}
	}

	Sys_Error ("Couldn't set video mode: %s", SDL_GetError ());
	SDL_Quit ();

success:
	viddef.numpages = 2;

	ctx->init_gl ();
}

static void
sdlgl_end_rendering (void)
{
	qfglFinish ();
	SDL_GL_SwapBuffers ();
}

static void
sdl_load_gl (void)
{
	if (SDL_GL_LoadLibrary (gl_driver) != 0)
		Sys_Error ("Couldn't load OpenGL library %s!", gl_driver);

	use_gl_procaddress = 1;

	qfglFinish = QFGL_ProcAddress ("glFinish", true);
}

gl_ctx_t *
SDL_GL_Context (void)
{
	gl_ctx_t *ctx = calloc (1, sizeof (gl_ctx_t));
	ctx->load_gl = sdl_load_gl;
	ctx->create_context = sdlgl_set_vid_mode;
	ctx->get_proc_address = QFGL_ProcAddress;
	ctx->end_rendering = sdlgl_end_rendering;
	return ctx;
}

void
SDL_GL_Init_Cvars ()
{
	Cvar_Register (&gl_driver_cvar, 0, 0);
}
