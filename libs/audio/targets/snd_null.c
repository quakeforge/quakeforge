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

#include "QF/cvar.h"
#include "QF/sound.h"
#include "QF/plugin.h"

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

plugin_t           plugin_info;
plugin_data_t      plugin_info_data;
plugin_funcs_t     plugin_info_funcs;
general_data_t     plugin_info_general_data;
general_funcs_t    plugin_info_general_funcs;
sound_data_t       plugin_info_sound_data;
sound_funcs_t      plugin_info_sound_funcs;

void SND_Init (void);
void SND_Shutdown (void);
void SND_AmbientOff (void);
void SND_AmbientOn (void);
void SND_TouchSound (char *sample);
void SND_ClearBuffer (void);
void SND_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void SND_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation);
void SND_StopSound (int entnum, int entchannel);
sfx_t *SND_PrecacheSound (char *sample);
void SND_ClearPrecache (void);
void SND_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up);
void SND_StopAllSounds (qboolean clear);
void SND_BeginPrecaching (void);
void SND_EndPrecaching (void);
void SND_ExtraUpdate (void);
void SND_LocalSound (char *s);

void SND_Init_Cvars (void);

void
SND_Init (void)
{
	SND_Init_Cvars ();
}

void
SND_Init_Cvars (void)
{
	volume = Cvar_Get ("volume", "0.7", CVAR_ARCHIVE, NULL,
			"Volume level of sounds");
	loadas8bit =
		Cvar_Get ("loadas8bit", "0", CVAR_NONE, NULL, "Load samples as 8-bit");
	bgmvolume = Cvar_Get ("bgmvolume", "1", CVAR_ARCHIVE, NULL,
			"CD music volume");
        snd_interp = Cvar_Get ("snd_interp", "1", CVAR_ARCHIVE, NULL,
                                  "control sample interpolation");
}

void
SND_AmbientOff (void)
{
}

void
SND_AmbientOn (void)
{
}

void
SND_Shutdown (void)
{
}

void
SND_TouchSound (char *sample)
{
}

void
SND_ClearBuffer (void)
{
}

void
SND_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
}

void
SND_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,
			  float attenuation)
{
}

void
SND_StopSound (int entnum, int entchannel)
{
}

sfx_t      *
SND_PrecacheSound (char *sample)
{
	return NULL;
}

void
SND_ClearPrecache (void)
{
}

void
SND_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up)
{
}

void
SND_StopAllSounds (qboolean clear)
{
}

void
SND_BeginPrecaching (void)
{
}

void
SND_EndPrecaching (void)
{
}

void
SND_ExtraUpdate (void)
{
}

void
SND_LocalSound (char *s)
{
}

plugin_t *
PluginInfo (void) {
    plugin_info.type = qfp_sound;
    plugin_info.api_version = QFPLUGIN_VERSION;
    plugin_info.plugin_version = "0.1";
    plugin_info.description = "ALSA 0.5.x digital output";
    plugin_info.copyright = "Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001  contributors of the QuakeForge project\n"
		"Please see the file \"AUTHORS\" for a list of contributors";
    plugin_info.functions = &plugin_info_funcs;
    plugin_info.data = &plugin_info_data;

    plugin_info_data.general = &plugin_info_general_data;
    plugin_info_data.input = NULL;
    plugin_info_data.sound = &plugin_info_sound_data;

    plugin_info_funcs.general = &plugin_info_general_funcs;
    plugin_info_funcs.input = NULL;
    plugin_info_funcs.sound = &plugin_info_sound_funcs;

    plugin_info_general_funcs.p_Init = SND_Init;
    plugin_info_general_funcs.p_Shutdown = SND_Shutdown;

    plugin_info_sound_funcs.pS_AmbientOff = SND_AmbientOff;
    plugin_info_sound_funcs.pS_AmbientOn = SND_AmbientOn;
    plugin_info_sound_funcs.pS_TouchSound = SND_TouchSound;
    plugin_info_sound_funcs.pS_ClearBuffer = SND_ClearBuffer;
    plugin_info_sound_funcs.pS_StaticSound = SND_StaticSound;
    plugin_info_sound_funcs.pS_StartSound = SND_StartSound;
    plugin_info_sound_funcs.pS_StopSound = SND_StopSound;
    plugin_info_sound_funcs.pS_PrecacheSound = SND_PrecacheSound;
    plugin_info_sound_funcs.pS_ClearPrecache = SND_ClearPrecache;
    plugin_info_sound_funcs.pS_Update = SND_Update;
    plugin_info_sound_funcs.pS_StopAllSounds = SND_StopAllSounds;
    plugin_info_sound_funcs.pS_BeginPrecaching = SND_BeginPrecaching;
    plugin_info_sound_funcs.pS_EndPrecaching = SND_EndPrecaching;
    plugin_info_sound_funcs.pS_ExtraUpdate = SND_ExtraUpdate;
    plugin_info_sound_funcs.pS_LocalSound = SND_LocalSound;

    return &plugin_info;
}
