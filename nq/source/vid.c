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

#include "cvar.h"
#include "vid.h"
#include "va.h"
#include "qargs.h"
#include "sys.h"

extern viddef_t vid;					// global video state

int         scr_width, scr_height;
cvar_t     *vid_width;
cvar_t     *vid_height;

void
VID_GetWindowSize (int def_w, int def_h)
{
	int         pnum;

	vid_width =
		Cvar_Get ("vid_width", va ("%d", def_w), CVAR_ROM, "screen width");
	vid_height =
		Cvar_Get ("vid_height", va ("%d", def_h), CVAR_ROM, "screen height");

	if ((pnum = COM_CheckParm ("-width"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -width <width>\n");
		Cvar_SetROM (vid_width, com_argv[pnum + 1]);
		if (!vid_width->int_val)
			Sys_Error ("VID: Bad window width\n");
	}

	if ((pnum = COM_CheckParm ("-height"))) {
		if (pnum >= com_argc - 1)
			Sys_Error ("VID: -height <height>\n");
		Cvar_SetROM (vid_height, com_argv[pnum + 1]);
		if (!vid_height->int_val)
			Sys_Error ("VID: Bad window height\n");
	}

	if ((pnum = COM_CheckParm ("-winsize"))) {
		if (pnum >= com_argc - 2)
			Sys_Error ("VID: -winsize <width> <height>\n");
		Cvar_SetROM (vid_width, com_argv[pnum + 1]);
		Cvar_SetROM (vid_height, com_argv[pnum + 2]);
		if (!vid_width->int_val || !vid_height->int_val)
			Sys_Error ("VID: Bad window width/height\n");
	}

	scr_width = vid.width = vid_width->int_val;
	scr_height = vid.height = vid_height->int_val;
}
