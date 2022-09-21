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

#ifndef __QF_vid_h
#define __QF_vid_h

#include "QF/listener.h"
#include "QF/qtypes.h"

#define VID_CBITS	6
#define VID_GRADES	(1 << VID_CBITS)

typedef struct {
	qboolean		 initialized;
	qboolean		 is8bit;
	byte			*gammatable;	// 256
	const byte      *basepal;		// 256 * 3
	byte            *palette;		// 256 * 3
	byte            *palette32;		// 256 * 4 includes alpha
	byte			*colormap8;		// 256 * VID_GRADES size
	unsigned short	*colormap16;	// 256 * VID_GRADES size
	unsigned int	*colormap32;	// 256 * VID_GRADES size
	int				 fullbright;	// index of first fullbright color
	unsigned		 width;
	unsigned		 height;
	int				 numpages;
	qboolean		 recalc_refdef;	// if true, recalc vid-based stuff
	struct vid_internal_s *vid_internal;

	struct viddef_listener_set_s *onPaletteChanged;
} viddef_t;

typedef struct viddef_listener_set_s LISTENER_SET_TYPE (viddef_t)
	viddef_listener_set_t;
typedef void (*viddef_listener_t) (void *data, const viddef_t *viddef);

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

void VID_OnPaletteChange_AddListener (viddef_listener_t listener, void *data);
void VID_OnPaletteChange_RemoveListener (viddef_listener_t listener,
										 void *data);

#endif//__QF_vid_h
