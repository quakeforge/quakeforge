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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "vid.h"
#include "QF/va.h"
#include "QF/qargs.h"
#include "QF/sys.h"

extern viddef_t vid;					// global video state

int         scr_width, scr_height;
cvar_t     *vid_width;
cvar_t     *vid_height;

void
VID_GetWindowSize (int def_w, int def_h)
{
	int pnum;

	vid_width = Cvar_Get ("vid_width", va ("%d", def_w), CVAR_NONE, NULL,
			"screen width");
	vid_height = Cvar_Get ("vid_height", va ("%d", def_h), CVAR_NONE, NULL,
			"screen height");

	if ((pnum = COM_CheckParm ("-width"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -width <width>\n");

		Cvar_Set (vid_width, com_argv[pnum + 1]);

		if (!vid_width->int_val)
			Sys_Error ("VID: Bad window width\n");
	}

	if ((pnum = COM_CheckParm ("-height"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -height <height>\n");

		Cvar_Set (vid_height, com_argv[pnum + 1]);

		if (!vid_height->int_val)
			Sys_Error ("VID: Bad window height\n");
	}

	if ((pnum = COM_CheckParm ("-winsize"))) {
		if (pnum >= com_argc - 2)
			Sys_Error ("VID: -winsize <width> <height>\n");

		Cvar_Set (vid_width, com_argv[pnum + 1]);
		Cvar_Set (vid_height, com_argv[pnum + 2]);

		if (!vid_width->int_val || !vid_height->int_val)
			Sys_Error ("VID: Bad window width/height\n");
	}

	Cvar_SetFlags (vid_width, vid_width->flags | CVAR_ROM);
	Cvar_SetFlags (vid_height, vid_height->flags | CVAR_ROM);

	scr_width = vid.width = vid_width->int_val;
	scr_height = vid.height = vid_height->int_val;
}

#if 0
VID_Calc_Gamma (void)
{
	float f;
	int i;
	int v;
	byte 
	float g = bound (0.3, gamma->value, 3);

	Cvar_SetValue (gamma, g);
	if (gamma_flipped->int_val)
		g = 1 / g;
	for (i = 0; i < 256; i++) {
		f = pow ((i + 1) / 256.0, g);
		v = f * 255 + 0.5;
		lightmap_gamma[i] = bound (0, v, 255);
		for (j = 0; j < 3; j++) {
			f = pow ((host_basepal[i * 3 + j] + 1) / 256.0, g);
			v = f * 255 + 0.5;
			palette[i] = bound (0, v, 255);
		}
	}
}
#endif
