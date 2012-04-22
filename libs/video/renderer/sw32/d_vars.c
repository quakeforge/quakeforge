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

#define NH_DEFINE
#include "namehack.h"

#include "QF/qtypes.h"

// all global and static refresh variables are collected in a contiguous block
// to avoid cache conflicts.

// global refresh variables -----------------------------

// FIXME: make into one big structure, like cl or sv
// FIXME: do separately for refresh engine and driver

float       sw32_d_sdivzstepu, sw32_d_tdivzstepu, sw32_d_zistepu;
float       sw32_d_sdivzstepv, sw32_d_tdivzstepv, sw32_d_zistepv;
float       sw32_d_sdivzorigin, sw32_d_tdivzorigin, sw32_d_ziorigin;

fixed16_t   sw32_sadjust, sw32_tadjust, sw32_bbextents, sw32_bbextentt;

byte       *sw32_cacheblock;
int         sw32_cachewidth;
byte       *sw32_d_viewbuffer;
short      *sw32_d_pzbuffer;
unsigned int sw32_d_zrowbytes;
unsigned int sw32_d_zwidth;
