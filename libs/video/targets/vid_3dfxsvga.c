/*
	vid_3dfxsvga.c

	OpenGL device driver for 3Dfx chipsets running Linux

	Copyright (C) 1996-1997  Id Software, Inc.
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
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <GL/gl.h>
#include <GL/fxmesa.h>
#include <glide/glide.h>
#include <glide/sst1vid.h>
#include <sys/signal.h>

#include "compat.h"
#include "QF/console.h"
#include "QF/cvar.h"
#include "glquake.h"
#include "QF/qargs.h"
#include "QF/qendian.h"
#include "qfgl_ext.h"
#include "sbar.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/vid.h"

#define WARP_WIDTH              320
#define WARP_HEIGHT             200

// FIXME!!!!! This belongs in include/qfgl_ext.h -- deek
typedef void (GLAPIENTRY * QF_3DfxSetDitherModeEXT) (GrDitherMode_t mode);

cvar_t      *vid_system_gamma;

static fxMesaContext fc = NULL;

int         VID_options_items = 0;

extern void GL_Init_Common (void);
extern void VID_Init8bitPalette (void);

/*-----------------------------------------------------------------------*/

void
VID_Shutdown (void)
{
	if (!fc)
		return;

	fxMesaDestroyContext (fc);
}

void
signal_handler (int sig)
{
	printf ("Received signal %d, exiting...\n", sig);
	Sys_Quit ();
	abort ();
	exit (0);
}

void
InitSig (void)
{
	signal (SIGHUP, signal_handler);
	signal (SIGINT, signal_handler);
	signal (SIGQUIT, signal_handler);
	signal (SIGILL, signal_handler);
	signal (SIGTRAP, signal_handler);
//  signal(SIGIOT, signal_handler);
	signal (SIGBUS, signal_handler);
//  signal(SIGFPE, signal_handler);
	signal (SIGSEGV, signal_handler);
	signal (SIGTERM, signal_handler);
}

/*
	GL_Init
*/
void
GL_Init (void)
{
	QF_3DfxSetDitherModeEXT dither_select = NULL;
	int p;
	
	GL_Init_Common ();

	if (!(QFGL_ExtensionPresent ("3DFX_set_dither_mode")))
		return;

	if (!(dither_select = QFGL_ExtensionAddress ("gl3DfxSetDitherModeEXT")))
		return;

	Con_Printf ("Dithering: ");

	if ((p = COM_CheckParm ("-dither")) && p < com_argc) {
		if (strequal (com_argv[p+1], "2x2")) {
			dither_select (GR_DITHER_2x2);
			Con_Printf ("2x2.\n");
		}
		if (strequal (com_argv[p+1], "4x4")) {
			dither_select (GR_DITHER_4x4);
			Con_Printf ("4x4.\n");
		}
	} else {
		glDisable (GL_DITHER);
		Con_Printf ("disabled.\n");
	}
}

void
GL_EndRendering (void)
{
	glFlush ();
	fxMesaSwapBuffers ();
	Sbar_Changed ();
}

static int  resolutions[][3] = {
	{320, 200, GR_RESOLUTION_320x200},
	{320, 240, GR_RESOLUTION_320x240},
	{400, 256, GR_RESOLUTION_400x256},
	{400, 300, GR_RESOLUTION_400x300},
	{512, 256, GR_RESOLUTION_512x256},
	{512, 384, GR_RESOLUTION_512x384},
	{640, 200, GR_RESOLUTION_640x200},
	{640, 350, GR_RESOLUTION_640x350},
	{640, 400, GR_RESOLUTION_640x400},
	{640, 480, GR_RESOLUTION_640x480},
	{800, 600, GR_RESOLUTION_800x600},
	{856, 480, GR_RESOLUTION_856x480},
	{960, 720, GR_RESOLUTION_960x720},
#ifdef GR_RESOLUTION_1024x768
	{1024, 768, GR_RESOLUTION_1024x768},
#endif
#ifdef GR_RESOLUTION_1152x864
	{1152, 864, GR_RESOLUTION_1152x864},
#endif
#ifdef GR_RESOLUTION_1280x960
	{1280, 960, GR_RESOLUTION_1280x960},
#endif
#ifdef GR_RESOLUTION_1280x1024
	{1280, 1024, GR_RESOLUTION_1280x1024},
#endif
#ifdef GR_RESOLUTION_1600x1024
	{1600, 1024, GR_RESOLUTION_1600x1024},
#endif
#ifdef GR_RESOLUTION_1600x1200
	{1600, 1200, GR_RESOLUTION_1600x1200},
#endif
#ifdef GR_RESOLUTION_1792x1344
	{1792, 1344, GR_RESOLUTION_1792x1344},
#endif
#ifdef GR_RESOLUTION_1856x1392
	{1856, 1392, GR_RESOLUTION_1856x1392},
#endif
#ifdef GR_RESOLUTION_1920x1440
	{1920, 1440, GR_RESOLUTION_1920x1440},
#endif
#ifdef GR_RESOLUTION_2048x1536
	{2048, 1536, GR_RESOLUTION_2048x1536},
#endif
#ifdef GR_RESOLUTION_2048x2048
	{2048, 2048, GR_RESOLUTION_2048x2048}
#endif
};

#define NUM_RESOLUTIONS		(sizeof (resolutions) / (sizeof (int) * 3))


static int
findres (int *width, int *height)
{
	int         i;

	for (i = 0; i < NUM_RESOLUTIONS; i++) {
		if ((*width <= resolutions[i][0]) && (*height <= resolutions[i][1])) {
			*width = resolutions[i][0];
			*height = resolutions[i][1];
			return resolutions[i][2];
		}
	}

	*width = 640;
	*height = 480;
	return GR_RESOLUTION_640x480;
}

void
VID_Init (unsigned char *palette)
{
	int         i;
	GLint       attribs[32];

	VID_GetWindowSize (640, 480);
	Con_CheckResize (); // Now that we have a window size, fix console

	vid.maxwarpwidth = WARP_WIDTH;
	vid.maxwarpheight = WARP_HEIGHT;
	vid.colormap = vid_colormap;
	vid.fullbright = 256 - LittleLong (*((int *) vid.colormap + 2048));

	// interpret command-line params

	// set vid parameters
	attribs[0] = FXMESA_DOUBLEBUFFER;
	attribs[1] = FXMESA_ALPHA_SIZE;
	attribs[2] = 1;
	attribs[3] = FXMESA_DEPTH_SIZE;
	attribs[4] = 1;
	attribs[5] = FXMESA_NONE;

	if ((i = COM_CheckParm ("-conwidth")))
		vid.conwidth = atoi (com_argv[i + 1]);
	else
		vid.conwidth = 640;

	vid.conwidth &= 0xfff8;				// make it a multiple of eight

	vid.conwidth = max (vid.conwidth, 320);

	// pick a conheight that matches with correct aspect
	vid.conheight = vid.conwidth * 3 / 4;

	if ((i = COM_CheckParm ("-conheight")) != 0)
		vid.conheight = atoi (com_argv[i + 1]);

	vid.conheight = max (vid.conheight, 200);

	fc = fxMesaCreateContext (0, findres (&scr_width, &scr_height),
							  GR_REFRESH_75Hz, attribs);
	if (!fc)
		Sys_Error ("Unable to create 3DFX context.\n");

	fxMesaMakeCurrent (fc);

	vid.width = vid.conwidth = min (vid.conwidth, scr_width);
	vid.height = vid.conheight = min (vid.conheight, scr_height);

	vid.aspect = ((float) vid.height / (float) vid.width) * (320.0 / 240.0);
	vid.numpages = 2;

	InitSig ();							// trap evil signals

	vid_gamma_avail = 1;
	
	GL_Init ();

	VID_InitGamma (palette);
	VID_SetPalette (palette);

	// Check for 3DFX Extensions and initialize them.
	VID_Init8bitPalette ();

	vid.initialized = true;

	Con_Printf ("Video mode %dx%d initialized.\n", scr_width, scr_height);

	vid.recalc_refdef = 1;				// force a surface cache flush
}

void
VID_Init_Cvars (void)
{
	vid_system_gamma = Cvar_Get ("vid_system_gamma", "1", CVAR_ARCHIVE, NULL, "Use system gamma control if available");
}

void
VID_ExtraOptionDraw (unsigned int options_draw_cursor)
{
/* Port specific Options menu entrys */
}

void
VID_ExtraOptionCmd (int option_cursor)
{
/*
	switch(option_cursor)
	{
	case 12:  // Always start with 12
	break;
	}
*/
}

void
VID_SetCaption (char *text)
{
}

qboolean
VID_SetGamma (double gamma)
{
	 // FIXME: Need function for HW gamma correction
	 // return X11_SetGamma (gamma);
	grGammaCorrectionValue((float) gamma);
	return true;
	 
}

