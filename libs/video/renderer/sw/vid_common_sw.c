/*
	vid_common_sw.c

	Common software video driver functions

	Copyright (C) 2001 Jeff Teunissen <deek@quakeforge.net>

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#include <stdlib.h>
#include <math.h>

#include "QF/console.h"
#include "QF/mathlib.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "vid_internal.h"

unsigned short d_8to16table[256];

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
	if (viddef.surf_cache_size)
		cachesize = viddef.surf_cache_size (viddef.width, viddef.height);

	// Free the old z-buffer
	if (viddef.zbuffer) {
		free (viddef.zbuffer);
		viddef.zbuffer = NULL;
	}
	// Free the old surface cache
	if (viddef.surfcache) {
		if (viddef.flush_caches)
			viddef.flush_caches ();
		free (viddef.surfcache);
		viddef.surfcache = NULL;
	}
	if (viddef.do_screen_buffer) {
		viddef.do_screen_buffer ();
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

	if (viddef.init_caches)
		viddef.init_caches (viddef.surfcache, cachesize);
}

void
VID_ShiftPalette (unsigned char *p)
{
	VID_SetPalette (p);
}
