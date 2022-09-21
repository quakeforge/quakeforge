/*
	vid.c

	general video driver functions

	Copyright (C) 1996-1997 Id Software, Inc.

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
#include <math.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/mathlib.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/va.h"
#include "QF/ui/view.h"

#include "compat.h"
#include "d_iface.h"
#include "vid_internal.h"

/* Software and hardware gamma support */
#define viddef (*r_data->vid)
#define vi (viddef.vid_internal)
float vid_gamma;
static cvar_t vid_gamma_cvar = {
	.name = "vid_gamma",
	.description =
		"Gamma correction",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &vid_gamma },
};
int vid_system_gamma;
static cvar_t vid_system_gamma_cvar = {
	.name = "vid_system_gamma",
	.description =
		"Use system gamma control if available",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &vid_system_gamma },
};
qboolean	vid_gamma_avail;		// hardware gamma availability

VISIBLE unsigned int	d_8to24table[256];

/* Screen size */
int vid_width;
static cvar_t vid_width_cvar = {
	.name = "vid_width",
	.description =
		"screen width",
	.default_value = 0,
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &vid_width },
};
int vid_height;
static cvar_t vid_height_cvar = {
	.name = "vid_height",
	.description =
		"screen height",
	.default_value = 0,
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &vid_height },
};

int vid_fullscreen;
static cvar_t vid_fullscreen_cvar = {
	.name = "vid_fullscreen",
	.description =
		"Toggles fullscreen mode",
	.default_value = "0",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &vid_fullscreen },
};

void
VID_GetWindowSize (int def_w, int def_h)
{
	int pnum;

	vid_width_cvar.default_value = nva ("%d", def_w);
	vid_height_cvar.default_value = nva ("%d", def_h);
	Cvar_Register (&vid_width_cvar, 0, 0);
	Cvar_Register (&vid_height_cvar, 0, 0);

	if ((pnum = COM_CheckParm ("-width"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -width <width>");

		vid_width = atoi (com_argv[pnum + 1]);
		if (!vid_width)
			Sys_Error ("VID: Bad window width");
	}

	if ((pnum = COM_CheckParm ("-height"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -height <height>");

		vid_height = atoi (com_argv[pnum + 1]);
		if (!vid_height)
			Sys_Error ("VID: Bad window height");
	}

	if ((pnum = COM_CheckParm ("-winsize"))) {
		if (pnum >= com_argc - 2)
			Sys_Error ("VID: -winsize <width> <height>");

		vid_width = atoi (com_argv[pnum + 1]);
		vid_height = atoi (com_argv[pnum + 2]);

		if (!vid_width || !vid_height)
			Sys_Error ("VID: Bad window width/height");
	}


	// viddef.maxlowwidth = LOW_WIDTH;
	// viddef.maxlowheight = LOW_HEIGHT;

	viddef.width = vid_width;
	viddef.height = vid_height;
}

void
VID_SetWindowSize (int width, int height)
{
	if (width < 0 || height < 0) {
		Sys_Error ("VID_SetWindowSize: invalid size: %d, %d", width, height);
	}
	if (width != (int) viddef.width || height != (int) viddef.height) {
		viddef.width = width;
		viddef.height = height;
		if (viddef.onVidResize) {
			LISTENER_INVOKE (viddef.onVidResize, &viddef);
		}
	}
}

/* GAMMA FUNCTIONS */

static void
VID_BuildGammaTable (double gamma)
{
	int 	i;

	if (gamma == 1.0) { // linear, don't bother with the math
		for (i = 0; i < 256; i++) {
			viddef.gammatable[i] = i;
		}
	} else {
		double	g = 1.0 / gamma;
		int 	v;

		for (i = 0; i < 256; i++) { // Build/update gamma lookup table
			v = (int) ((255.0 * pow ((double) i / 255.0, g)) + 0.5);
			viddef.gammatable[i] = bound (0, v, 255);
		}
	}
}

void
VID_UpdateGamma (void)
{
	byte       *p24;
	byte       *p32;
	const byte *col;
	int         i;

	viddef.recalc_refdef = 1;				// force a surface cache flush

	if (vid_gamma_avail && vid_system_gamma) {	// Have system, use it
		Sys_MaskPrintf (SYS_vid, "Setting hardware gamma to %g\n", vid_gamma);
		VID_BuildGammaTable (1.0);	// hardware gamma wants a linear palette
		VID_SetGamma (vid_gamma);
		p24 = viddef.palette;
		p32 = viddef.palette32;
		col = viddef.basepal;
		for (i = 0; i < 256; i++) {
			*p32++ = *p24++ = *col++;
			*p32++ = *p24++ = *col++;
			*p32++ = *p24++ = *col++;
			*p32++ = 255;
		}
		p32[-1] = 0;	// color 255 is transparent
	} else {	// We have to hack the palette
		Sys_MaskPrintf (SYS_vid, "Setting software gamma to %g\n", vid_gamma);
		VID_BuildGammaTable (vid_gamma);
		p24 = viddef.palette;
		p32 = viddef.palette32;
		col = viddef.basepal;
		for (i = 0; i < 256; i++) {
			*p32++ = *p24++ = viddef.gammatable[*col++];
			*p32++ = *p24++ = viddef.gammatable[*col++];
			*p32++ = *p24++ = viddef.gammatable[*col++];
			*p32++ = 255;
		}
		p32[-1] = 0;	// color 255 is transparent
		// update with the new palette
		vi->set_palette (vi->data, viddef.palette);
	}
}

static void
vid_gamma_f (void *data, const cvar_t *cvar)
{
	vid_gamma = bound (0.1, vid_gamma, 9.9);
	VID_UpdateGamma ();
}

/*
	VID_InitGamma

	Initialize the vid_gamma Cvar, and set up the palette
*/
void
VID_InitGamma (const byte *pal)
{
	int 	i;
	double	gamma = 1.0;

	viddef.gammatable = malloc (256);
	viddef.basepal = pal;
	viddef.palette = malloc (256 * 3);
	viddef.palette32 = malloc (256 * 4);
	if ((i = COM_CheckParm ("-gamma"))) {
		gamma = atof (com_argv[i + 1]);
	}
	gamma = bound (0.1, gamma, 9.9);

	Cvar_Register (&vid_gamma_cvar, vid_gamma_f, 0);

	VID_BuildGammaTable (vid_gamma);

	if (viddef.onPaletteChanged) {
		LISTENER_INVOKE (viddef.onPaletteChanged, &viddef);
	}
}

void
VID_ClearMemory (void)
{
	if (vi->flush_caches) {
		vi->flush_caches (vi->data);
	}
}

VISIBLE void
VID_OnPaletteChange_AddListener (viddef_listener_t listener, void *data)
{
	if (!viddef.onPaletteChanged) {
		viddef.onPaletteChanged = malloc (sizeof (*viddef.onPaletteChanged));
		LISTENER_SET_INIT (viddef.onPaletteChanged, 8);
	}
	LISTENER_ADD (viddef.onPaletteChanged, listener, data);
}

VISIBLE void
VID_OnPaletteChange_RemoveListener (viddef_listener_t listener, void *data)
{
	if (viddef.onPaletteChanged) {
		LISTENER_REMOVE (viddef.onPaletteChanged, listener, data);
	}
}

VISIBLE void
VID_Init (byte *palette, byte *colormap)
{
	vid_system.init (palette, colormap);
}

static void
vid_fullscreen_f (void *data, const cvar_t *var)
{
	vid_system.update_fullscreen (vid_fullscreen);
}

VISIBLE void
VID_Init_Cvars (void)
{
	if (vid_system.update_fullscreen) {
		// A bit of a hack, but windows registers a vid_fullscreen command
		// and does fullscreen handling differently.
		Cvar_Register (&vid_fullscreen_cvar, vid_fullscreen_f, 0);
	}
	Cvar_Register (&vid_system_gamma_cvar, 0, 0);
	vid_system.init_cvars ();
}
