/*
	snd_null.c

	include this instead of all the other snd_* files to have no sound
	code whatsoever

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

#include "qtypes.h"
#include "sound.h"

// =======================================================================
// Various variables also defined in snd_dma.c
// FIXME - should be put in one place
// =======================================================================
channel_t   channels[MAX_CHANNELS];
int         total_channels;
volatile dma_t *shm = 0;
cvar_t     *loadas8bit;
int         paintedtime;				// sample PAIRS


cvar_t     *bgmvolume;
cvar_t     *volume;


void
S_Init (void)
{
	S_Init_Cvars ();
}

void
S_Init_Cvars (void)
{
	volume = Cvar_Get ("volume", "0.7", CVAR_ARCHIVE, "Volume level of sounds");
	loadas8bit = Cvar_Get ("loadas8bit", "0", CVAR_NONE, "Load samples as 8-bit");
	bgmvolume = Cvar_Get ("bgmvolume", "1", CVAR_ARCHIVE, "CD music volume");
}

void
S_AmbientOff (void)
{
}

void
S_AmbientOn (void)
{
}

void
S_Shutdown (void)
{
}

void
S_TouchSound (char *sample)
{
}

void
S_ClearBuffer (void)
{
}

void
S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
}

void
S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,
			  float attenuation)
{
}

void
S_StopSound (int entnum, int entchannel)
{
}

sfx_t      *
S_PrecacheSound (char *sample)
{
	return NULL;
}

void
S_ClearPrecache (void)
{
}

void
S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up)
{
}

void
S_StopAllSounds (qboolean clear)
{
}

void
S_BeginPrecaching (void)
{
}

void
S_EndPrecaching (void)
{
}

void
S_ExtraUpdate (void)
{
}

void
S_LocalSound (char *s)
{
}
