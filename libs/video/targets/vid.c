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

#include "compat.h"
#include "vid_internal.h"

/* Software and hardware gamma support */
#define viddef (*r_data->vid)
byte       *vid_colormap;
cvar_t	   *vid_gamma;
cvar_t	   *vid_system_gamma;
cvar_t     *con_width; // FIXME: Try to move with rest of con code
cvar_t     *con_height; // FIXME: Try to move with rest of con code
qboolean	vid_gamma_avail;		// hardware gamma availability

VISIBLE unsigned int	d_8to24table[256];

/* Screen size */
cvar_t	   *vid_width;
cvar_t	   *vid_height;
cvar_t     *vid_aspect;

cvar_t     *vid_fullscreen;

static void
vid_aspect_f (cvar_t *var)
{
	const char *p = strchr (var->string, ':');
	float       w, h;

	if (p) {
		w = atof (var->string);
		h = atof (p + 1);
		if (w > 0.0 && h > 0.0) {
			var->vec[0] = w;
			var->vec[1] = h;
			return;
		}
	}
	Sys_Printf ("badly formed aspect ratio: %s. Using default 4:3\n",
				var->string);
	var->vec[0] = 4.0;
	var->vec[1] = 3.0;
}

void
VID_GetWindowSize (int def_w, int def_h)
{
	int pnum, conheight;

	vid_width = Cvar_Get ("vid_width", va ("%d", def_w), CVAR_NONE, NULL,
			"screen width");
	vid_height = Cvar_Get ("vid_height", va ("%d", def_h), CVAR_NONE, NULL,
			"screen height");
	vid_aspect = Cvar_Get ("vid_aspect", "4:3", CVAR_ROM, vid_aspect_f,
			"Physical screen aspect ratio in \"width:height\" format. "
			"Common values are 4:3, 5:3, 8:5, 16:9, but any width:height "
			"measurement will do (eg, 475:296.875 the approximate dimentions "
			"in mm of the display area of a certain monitor)");

	if ((pnum = COM_CheckParm ("-width"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -width <width>");

		Cvar_Set (vid_width, com_argv[pnum + 1]);

		if (!vid_width->int_val)
			Sys_Error ("VID: Bad window width");
	}

	if ((pnum = COM_CheckParm ("-height"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -height <height>");

		Cvar_Set (vid_height, com_argv[pnum + 1]);

		if (!vid_height->int_val)
			Sys_Error ("VID: Bad window height");
	}

	if ((pnum = COM_CheckParm ("-winsize"))) {
		if (pnum >= com_argc - 2)
			Sys_Error ("VID: -winsize <width> <height>");

		Cvar_Set (vid_width, com_argv[pnum + 1]);
		Cvar_Set (vid_height, com_argv[pnum + 2]);

		if (!vid_width->int_val || !vid_height->int_val)
			Sys_Error ("VID: Bad window width/height");
	}

	Cvar_SetFlags (vid_width, vid_width->flags | CVAR_ROM);
	Cvar_SetFlags (vid_height, vid_height->flags | CVAR_ROM);

	viddef.width = vid_width->int_val;
	viddef.height = vid_height->int_val;

	viddef.aspect = ((vid_aspect->vec[0] * viddef.height)
				  / (vid_aspect->vec[1] * viddef.width));

	con_width = Cvar_Get ("con_width", va ("%d", viddef.width), CVAR_NONE,
						  NULL, "console effective width (GL only)");
	if ((pnum = COM_CheckParm ("-conwidth"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -conwidth <width>");
		Cvar_Set (con_width, com_argv[pnum + 1]);
	}
	// make con_width a multiple of 8 and >= 320
	Cvar_Set (con_width, va ("%d", max (con_width->int_val & ~7, 320)));
	Cvar_SetFlags (con_width, con_width->flags | CVAR_ROM);
	viddef.conwidth = con_width->int_val;

	conheight = (viddef.conwidth * vid_aspect->vec[1]) / vid_aspect->vec[0];
	con_height = Cvar_Get ("con_height", va ("%d", conheight), CVAR_NONE, NULL,
						   "console effective height (GL only)");
	if ((pnum = COM_CheckParm ("-conheight"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -conheight <width>");
		Cvar_Set (con_height, com_argv[pnum + 1]);
	}
	// make con_height >= 200
	Cvar_Set (con_height, va ("%d", max (con_height->int_val, 200)));
	Cvar_SetFlags (con_height, con_height->flags | CVAR_ROM);
	viddef.conheight = con_height->int_val;

	Con_CheckResize ();     // Now that we have a window size, fix console
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

/*
	VID_UpdateGamma

	This is a callback to update the palette or system gamma whenever the
	vid_gamma Cvar is changed.
*/
void
VID_UpdateGamma (cvar_t *vid_gamma)
{
	double gamma = bound (0.1, vid_gamma->value, 9.9);
	byte       *p24;
	byte       *p32;
	byte       *col;
	int         i;

	viddef.recalc_refdef = 1;				// force a surface cache flush

	if (vid_gamma_avail && vid_system_gamma->int_val) {	// Have system, use it
		Sys_MaskPrintf (SYS_VID, "Setting hardware gamma to %g\n", gamma);
		VID_BuildGammaTable (1.0);	// hardware gamma wants a linear palette
		VID_SetGamma (gamma);
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
		Sys_MaskPrintf (SYS_VID, "Setting software gamma to %g\n", gamma);
		VID_BuildGammaTable (gamma);
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
		viddef.vid_internal->set_palette (viddef.palette); // update with the new palette
	}
}

/*
	VID_InitGamma

	Initialize the vid_gamma Cvar, and set up the palette
*/
void
VID_InitGamma (unsigned char *pal)
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

	vid_gamma = Cvar_Get ("vid_gamma", va ("%f", gamma), CVAR_ARCHIVE,
						  VID_UpdateGamma, "Gamma correction");

	VID_BuildGammaTable (vid_gamma->value);
}

void
VID_InitBuffers (void)
{
	int         buffersize, zbuffersize, cachesize = 1;

	// No console scaling in the sw renderer
	viddef.conwidth = viddef.width;
	viddef.conheight = viddef.height;
	Con_CheckResize ();

	// Calculate the sizes we want first
	buffersize = viddef.rowbytes * viddef.height;
	zbuffersize = viddef.width * viddef.height * sizeof (*viddef.zbuffer);
	if (viddef.vid_internal->surf_cache_size)
		cachesize = viddef.vid_internal->surf_cache_size (viddef.width,
														  viddef.height);

	// Free the old z-buffer
	if (viddef.zbuffer) {
		free (viddef.zbuffer);
		viddef.zbuffer = NULL;
	}
	// Free the old surface cache
	if (viddef.surfcache) {
		if (viddef.vid_internal->flush_caches)
			viddef.vid_internal->flush_caches ();
		free (viddef.surfcache);
		viddef.surfcache = NULL;
	}
	if (viddef.vid_internal->do_screen_buffer) {
		viddef.vid_internal->do_screen_buffer ();
	} else {
		// Free the old screen buffer
		if (viddef.buffer) {
			free (viddef.buffer);
			viddef.conbuffer = viddef.buffer = NULL;
		}
		// Allocate the new screen buffer
		viddef.conbuffer = viddef.buffer = calloc (buffersize, 1);
		if (!viddef.conbuffer) {
			Sys_Error ("Not enough memory for video mode");
		}
	}
	// Allocate the new z-buffer
	viddef.zbuffer = calloc (zbuffersize, 1);
	if (!viddef.zbuffer) {
		free (viddef.buffer);
		viddef.conbuffer = viddef.buffer = NULL;
		Sys_Error ("Not enough memory for video mode");
	}
	// Allocate the new surface cache; free the z-buffer if we fail
	viddef.surfcache = calloc (cachesize, 1);
	if (!viddef.surfcache) {
		free (viddef.buffer);
		free (viddef.zbuffer);
		viddef.conbuffer = viddef.buffer = NULL;
		viddef.zbuffer = NULL;
		Sys_Error ("Not enough memory for video mode");
	}

	if (viddef.vid_internal->init_caches)
		viddef.vid_internal->init_caches (viddef.surfcache, cachesize);
}

void
VID_ClearMemory (void)
{
	if (viddef.vid_internal->flush_caches)
		viddef.vid_internal->flush_caches ();
}
