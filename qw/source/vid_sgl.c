/*
	vid_sgl.c

	Video driver for OpenGL-using versions of SDL

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

	$Id$
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
#ifndef WIN32
# include <sys/signal.h>
#endif

#include <SDL.h>

#include "QF/compat.h"
#include "QF/console.h"
#include "glquake.h"
#include "host.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "sbar.h"
#include "QF/sys.h"
#include "QF/va.h"

#ifdef WIN32
/* FIXME: this is evil hack to get full DirectSound support with SDL */
# include <windows.h>
# include <SDL_syswm.h>
HWND 		mainwindow;
#endif

#define	WARP_WIDTH	320
#define	WARP_HEIGHT	200

static qboolean vid_initialized = false;

cvar_t     *vid_fullscreen;

int         VID_options_items = 1;
int         modestate;

extern void GL_Init_Common (void);
extern void VID_Init8bitPalette (void);

/*-----------------------------------------------------------------------*/

void
VID_Shutdown (void)
{
	if (!vid_initialized)
		return;

	Con_Printf ("VID_Shutdown\n");

	SDL_Quit ();
}

#ifndef WIN32
static void
signal_handler (int sig)
{
	printf ("Received signal %d, exiting...\n", sig);
	Sys_Quit ();
	exit (sig);
}

static void
InitSig (void)
{
	signal (SIGHUP, signal_handler);
	signal (SIGINT, signal_handler);
	signal (SIGQUIT, signal_handler);
	signal (SIGILL, signal_handler);
	signal (SIGTRAP, signal_handler);
	signal (SIGIOT, signal_handler);
	signal (SIGBUS, signal_handler);
//  signal(SIGFPE, signal_handler);
	signal (SIGSEGV, signal_handler);
	signal (SIGTERM, signal_handler);
}
#endif

void
GL_Init (void)
{
	GL_Init_Common ();
}

void
GL_EndRendering (void)
{
	glFlush ();
	SDL_GL_SwapBuffers ();
	Sbar_Changed ();
}

void
VID_Init (unsigned char *palette)
{
	Uint32      flags = SDL_OPENGL;
	int         i;

//        SDL_SysWMinfo info;

	VID_GetWindowSize (640, 480);

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = host_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap + 2048));

	// Interpret command-line params

	// Set vid parameters
	if ((i = COM_CheckParm ("-conwidth")) != 0)
		vid.conwidth = atoi (com_argv[i + 1]);
	else
		vid.conwidth = scr_width;

	vid.conwidth &= 0xfff8;				// make it a multiple of eight
	if (vid.conwidth < 320)
		vid.conwidth = 320;

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	i = COM_CheckParm ("-conheight");
	if (i != 0)							// Set console height, but no smaller 
										// than 200 px
		vid.conheight = max (atoi (com_argv[i + 1]), 200);

	// Initialize the SDL library 
	if (SDL_Init (SDL_INIT_VIDEO) < 0)
		Sys_Error ("Couldn't initialize SDL: %s\n", SDL_GetError ());

	// Check if we want fullscreen
	if (vid_fullscreen->value) {
		flags |= SDL_FULLSCREEN;
		// Don't annoy Mesa/3dfx folks
#ifndef WIN32
		// FIXME: Maybe this could be put in a different spot, but I don't
		// know where.
		// Anyway, it's to work around a 3Dfx Glide bug.
		SDL_ShowCursor (0);
		SDL_WM_GrabInput (SDL_GRAB_ON);
		setenv ("MESA_GLX_FX", "fullscreen", 1);
	} else {
		setenv ("MESA_GLX_FX", "window", 1);
#endif
	}

	// Setup GL Attributes
	SDL_GL_SetAttribute (SDL_GL_RED_SIZE, 1);
	SDL_GL_SetAttribute (SDL_GL_GREEN_SIZE, 1);
	SDL_GL_SetAttribute (SDL_GL_BLUE_SIZE, 1);
	SDL_GL_SetAttribute (SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute (SDL_GL_DEPTH_SIZE, 1);

	if (SDL_SetVideoMode (scr_width, scr_height, 8, flags) == NULL) {
		Sys_Error ("Couldn't set video mode: %s\n", SDL_GetError ());
		SDL_Quit ();
	}

	vid.height = vid.conheight = min (vid.conheight, scr_height);
	vid.width = vid.conwidth = min (vid.conwidth, scr_width);
	Con_CheckResize (); // Now that we have a window size, fix console

	vid.aspect = ((float) vid.height / (float) vid.width) * (320.0 / 240.0);
	vid.numpages = 2;

#ifndef WIN32
	InitSig ();							// trap evil signals
#endif

	GL_Init ();

	GL_CheckBrightness (palette);
	VID_SetPalette (palette);

	// Check for 3DFX Extensions and initialize them.
	VID_Init8bitPalette ();

	Con_Printf ("Video mode %dx%d initialized.\n", scr_width, scr_height);

	vid_initialized = true;

#ifdef WIN32
        // FIXME: EVIL thing - but needed for win32 until
        // SDL_sound works better - without this DirectSound fails.

//        SDL_GetWMInfo(&info);
//        mainwindow=info.window;
        mainwindow=GetActiveWindow();
#endif

	vid.recalc_refdef = 1;				// force a surface cache flush
}

void
VID_Init_Cvars ()
{
	vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ROM, "Toggles fullscreen mode");
}

void
VID_SetCaption (char *text)
{
	if (text && *text) {
		char       *temp = strdup (text);

		SDL_WM_SetCaption (va ("%s %s: %s", PROGRAM, VERSION, temp), NULL);
		free (temp);
	} else {
		SDL_WM_SetCaption (va ("%s %s", PROGRAM, VERSION), NULL);
	}
}
