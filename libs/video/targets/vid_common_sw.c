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

#include "QF/sys.h"
#include "QF/vid.h"

unsigned short d_8to16table[256];

void
VID_InitBuffers (void)
{
	int         buffersize, zbuffersize, cachesize = 1;

	// Calculate the sizes we want first
	buffersize = vid.rowbytes * vid.height;
	zbuffersize = vid.width * vid.height * sizeof (*vid.zbuffer);
	if (vid.surf_cache_size)
		cachesize = vid.surf_cache_size (vid.width, vid.height);

	// Free the old z-buffer
	if (vid.zbuffer) {
		free (vid.zbuffer);
		vid.zbuffer = NULL;
	}
	// Free the old surface cache
	if (vid.surfcache) {
		if (vid.flush_caches)
			vid.flush_caches ();
		free (vid.surfcache);
		vid.surfcache = NULL;
	}
	if (vid.do_screen_buffer) {
		vid.do_screen_buffer ();
	} else {
		// Free the old screen buffer
		if (vid.buffer) {
			free (vid.buffer);
			vid.conbuffer = vid.buffer = NULL;
		}
		// Allocate the new screen buffer
		vid.conbuffer = vid.buffer = calloc (buffersize, 1);
		if (!vid.conbuffer) {
			Sys_Error ("Not enough memory for video mode");
		}
	}
	// Allocate the new z-buffer
	vid.zbuffer = calloc (zbuffersize, 1);
	if (!vid.zbuffer) {
		free (vid.buffer);
		vid.conbuffer = vid.buffer = NULL;
		Sys_Error ("Not enough memory for video mode");
	}
	// Allocate the new surface cache; free the z-buffer if we fail
	vid.surfcache = calloc (cachesize, 1);
	if (!vid.surfcache) {
		free (vid.buffer);
		free (vid.zbuffer);
		vid.conbuffer = vid.buffer = NULL;
		vid.zbuffer = NULL;
		Sys_Error ("Not enough memory for video mode");
	}

	if (vid.init_caches)
		vid.init_caches (vid.surfcache, cachesize);
}

void
VID_ShiftPalette (unsigned char *p)
{
	VID_SetPalette (p);
}
