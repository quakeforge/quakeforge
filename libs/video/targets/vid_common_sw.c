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
	
	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <math.h>

#include "QF/vid.h"

void
VID_InitBuffers (void)
{
#if 0 //XXX not just yet
	int         buffersize, zbuffersize, cachesize;
	void       *vid_surfcache;

	// Calculate the sizes we want first
	buffersize = vid.rowbytes * vid.height;
	zbuffersize = vid.width * vid.height * sizeof (*d_pzbuffer);
	cachesize = D_SurfaceCacheForRes (vid.width, vid.height);

	// Free the old screen buffer
	if (vid.buffer) {
		free (vid.buffer);
		vid.conbuffer = vid.buffer = NULL;
	}
	// Free the old z-buffer
	if (d_pzbuffer) {
		free (d_pzbuffer);
		d_pzbuffer = NULL;
	}
	// Free the old surface cache
	vid_surfcache = D_SurfaceCacheAddress ();
	if (vid_surfcache) {
		D_FlushCaches ();
		free (vid_surfcache);
		vid_surfcache = NULL;
	}
	// Allocate the new screen buffer
	vid.conbuffer = vid.buffer = calloc (buffersize, 1);
	if (!vid.conbuffer) {
		Sys_Error ("Not enough memory for video mode\n");
	}
	// Allocate the new z-buffer
	d_pzbuffer = calloc (zbuffersize, 1);
	if (!d_pzbuffer) {
		free (vid.buffer);
		vid.conbuffer = vid.buffer = NULL;
		Sys_Error ("Not enough memory for video mode\n");
	}
	// Allocate the new surface cache; free the z-buffer if we fail
	vid_surfcache = calloc (cachesize, 1);
	if (!vid_surfcache) {
		free (vid.buffer);
		free (d_pzbuffer);
		vid.conbuffer = vid.buffer = NULL;
		d_pzbuffer = NULL;
		Sys_Error ("Not enough memory for video mode\n");
	}

	D_InitCaches (vid_surfcache, cachesize);
#endif
}
