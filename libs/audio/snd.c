/*
	snd_plugin.c

	sound plugin wrapper

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

#include "QF/qtypes.h"
#include "QF/sound.h"
#include "QF/plugin.h"
#include "QF/console.h"
#include "QF/model.h"

// =======================================================================
// Various variables also defined in snd_dma.c
// FIXME - should be put in one place
// =======================================================================
extern channel_t       channels[MAX_CHANNELS];
extern int             total_channels;
extern volatile dma_t *shm;
extern cvar_t         *loadas8bit;
extern int             paintedtime;				// sample PAIRS
extern qboolean        snd_initialized;

extern cvar_t         *bgmvolume;
extern cvar_t         *volume;
extern cvar_t         *snd_interp;

cvar_t                *snd_plugin;
plugin_t              *sndmodule = NULL;

// FIXME: ewwwies
extern double host_frametime; // From host.h

extern struct model_s       **snd_worldmodel;
extern int snd_viewentity;

void
S_Init (void)
{
	S_Init_Cvars ();
	sndmodule = PI_LoadPlugin ("sound", snd_plugin->string);
	if (!sndmodule) {
		Con_Printf ("Loading of sound module: %s failed!\n", snd_plugin->string);
	}
	else {
		sndmodule->data->sound->worldmodel = &snd_worldmodel;
		sndmodule->data->sound->viewentity = &snd_viewentity;
		sndmodule->data->sound->host_frametime = &host_frametime;
		sndmodule->functions->general->p_Init ();
	}
}

void
S_Init_Cvars (void)
{
	volume = Cvar_Get ("volume", "0.7", CVAR_ARCHIVE, NULL,
			"Volume level of sounds");
	loadas8bit =
		Cvar_Get ("loadas8bit", "0", CVAR_NONE, NULL, "Load samples as 8-bit");
	bgmvolume = Cvar_Get ("bgmvolume", "1", CVAR_ARCHIVE, NULL,
			"CD music volume");
	snd_interp = Cvar_Get ("snd_interp", "1", CVAR_ARCHIVE, NULL,
	                              "control sample interpolation");
	snd_plugin = Cvar_Get ("snd_plugin", "null", CVAR_ARCHIVE, NULL,
			"Sound Plugin to use");
}

void
S_AmbientOff (void)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_AmbientOff ();
}

void
S_AmbientOn (void)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_AmbientOn ();
}

void
S_Shutdown (void)
{
	if (sndmodule) {
		sndmodule->functions->general->p_Shutdown ();
		PI_UnloadPlugin (sndmodule);
		sndmodule = NULL;
	}
}

void
S_TouchSound (char *sample)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_TouchSound (sample);
}

void
S_ClearBuffer (void)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_ClearBuffer ();
}

void
S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_StaticSound (sfx, origin, vol, attenuation);
}

void
S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,
			  float attenuation)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_StartSound (entnum, entchannel, sfx, origin, fvol, attenuation);
}

void
S_StopSound (int entnum, int entchannel)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_StopSound (entnum, entchannel);
}

sfx_t      *
S_PrecacheSound (char *sample)
{
	if (sndmodule)
		return sndmodule->functions->sound->pS_PrecacheSound (sample);
	else
		return NULL;
}

void
S_ClearPrecache (void)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_ClearPrecache ();
}

void
S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_Update (origin, v_forward, v_right, v_up);
}

void
S_StopAllSounds (qboolean clear)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_StopAllSounds (clear);
}

void
S_BeginPrecaching (void)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_BeginPrecaching ();
}

void
S_EndPrecaching (void)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_EndPrecaching ();
}

void
S_ExtraUpdate (void)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_ExtraUpdate ();
}

void
S_LocalSound (char *s)
{
	if (sndmodule)
		sndmodule->functions->sound->pS_LocalSound (s);
}
