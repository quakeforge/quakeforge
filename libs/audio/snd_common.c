/*
	snd_common.c

	common externally visible sound variables

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

#include "QF/cvar.h"
#include "QF/sound.h"

channel_t   channels[MAX_CHANNELS];
int         total_channels;
volatile dma_t *shm = 0;
cvar_t     *loadas8bit;
int         paintedtime;				// sample PAIRS
qboolean    snd_initialized = false;

cvar_t     *bgmvolume;
cvar_t     *volume;
cvar_t     *snd_interp;

