/*
	d_vars.c

	global refresh variables

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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/qtypes.h"

#ifdef PIC
# undef USE_INTEL_ASM //XXX asm pic hack
#endif

#ifndef USE_INTEL_ASM

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

// global refresh variables -----------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver

float       d_sdivzstepu, d_tdivzstepu, d_zistepu;
float       d_sdivzstepv, d_tdivzstepv, d_zistepv;
float       d_sdivzorigin, d_tdivzorigin, d_ziorigin;

fixed16_t   sadjust, tadjust, bbextents, bbextentt;

byte       *cacheblock;
int         cachewidth;
byte       *d_viewbuffer;
short      *d_zbuffer;
int         d_rowbytes;
int         d_zrowbytes;
int         d_zwidth;
int         d_height;

#endif // !USE_INTEL_ASM
