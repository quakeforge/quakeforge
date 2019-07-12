/*
	vid.h

	video driver defs

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

#ifndef __vid_h_
#define __vid_h_

#include "QF/qtypes.h"
#include "QF/vrect.h"

#define VID_CBITS	6
#define VID_GRADES	(1 << VID_CBITS)

typedef struct {
	qboolean		 initialized;
	qboolean		 is8bit;
	void			*buffer;		// invisible buffer
	short			*zbuffer;
	void			*surfcache;
	byte			*gammatable;	// 256
	byte            *basepal;		// 256 * 3
	byte            *palette;		// 256 * 3
	byte			*colormap8;		// 256 * VID_GRADES size
	unsigned short	*colormap16;	// 256 * VID_GRADES size
	unsigned int	*colormap32;	// 256 * VID_GRADES size
	int				 fullbright;	// index of first fullbright color
	int				 rowbytes;		// may be > width if displayed in a window
	int				 width;
	int				 height;
	float			 aspect;	// width / height -- < 1 is taller than wide
	int				 numpages;
	qboolean		 recalc_refdef;	// if true, recalc vid-based stuff
	qboolean		 cshift_changed;
	quat_t           cshift_color;
	void			*conbuffer;
	int				 conrowbytes;
	int				 conwidth;
	int				 conheight;
	byte			*direct;		// direct drawing to framebuffer, if not
									//  NULL
	struct vid_internal_s *vid_internal;
} viddef_t;

#define viddef (*r_data->vid)

extern unsigned int 	d_8to24table[256];	//FIXME nq/qw uses

extern qboolean			vid_gamma_avail;

void VID_Init_Cvars (void);

// Called at startup to set up translation tables, takes 256 8 bit RGB values
// the palette data will go away after the call, so it must be copied off if
// the video driver will need it again
void VID_Init (byte *palette, byte *colormap);
void VID_SetCaption (const char *text);
void VID_ClearMemory (void);

#endif	// __vid_h_
