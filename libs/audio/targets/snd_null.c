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

void I_S_Init (void);
void I_S_Shutdown (void);
void I_S_AmbientOff (void);
void I_S_AmbientOn (void);
void I_S_TouchSound (char *sample);
void I_S_ClearBuffer (void);
void I_S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation);
void I_S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,  float attenuation);
void I_S_StopSound (int entnum, int entchannel);
sfx_t *I_S_PrecacheSound (char *sample);
void I_S_ClearPrecache (void);
void I_S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up);
void I_S_StopAllSounds (qboolean clear);
void I_S_BeginPrecaching (void);
void I_S_EndPrecaching (void);
void I_S_ExtraUpdate (void);
void I_S_LocalSound (char *s);

void I_S_Init_Cvars (void);

void
I_S_Init (void)
{
	I_S_Init_Cvars ();
}

void
I_S_Init_Cvars (void)
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
I_S_AmbientOff (void)
{
}

void
I_S_AmbientOn (void)
{
}

void
I_S_Shutdown (void)
{
}

void
I_S_TouchSound (char *sample)
{
}

void
I_S_ClearBuffer (void)
{
}

void
I_S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
}

void
I_S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin, float fvol,
			  float attenuation)
{
}

void
I_S_StopSound (int entnum, int entchannel)
{
}

sfx_t      *
I_S_PrecacheSound (char *sample)
{
	return NULL;
}

void
I_S_ClearPrecache (void)
{
}

void
I_S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up)
{
}

void
I_S_StopAllSounds (qboolean clear)
{
}

void
I_S_BeginPrecaching (void)
{
}

void
I_S_EndPrecaching (void)
{
}

void
I_S_ExtraUpdate (void)
{
}

void
I_S_LocalSound (char *s)
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

    plugin_info_general_funcs.p_Init = I_S_Init;
    plugin_info_general_funcs.p_Shutdown = I_S_Shutdown;

    plugin_info_sound_funcs.pS_AmbientOff = I_S_AmbientOff;
    plugin_info_sound_funcs.pS_AmbientOn = I_S_AmbientOn;
    plugin_info_sound_funcs.pS_TouchSound = I_S_TouchSound;
    plugin_info_sound_funcs.pS_ClearBuffer = I_S_ClearBuffer;
    plugin_info_sound_funcs.pS_StaticSound = I_S_StaticSound;
    plugin_info_sound_funcs.pS_StartSound = I_S_StartSound;
    plugin_info_sound_funcs.pS_StopSound = I_S_StopSound;
    plugin_info_sound_funcs.pS_PrecacheSound = I_S_PrecacheSound;
    plugin_info_sound_funcs.pS_ClearPrecache = I_S_ClearPrecache;
    plugin_info_sound_funcs.pS_Update = I_S_Update;
    plugin_info_sound_funcs.pS_StopAllSounds = I_S_StopAllSounds;
    plugin_info_sound_funcs.pS_BeginPrecaching = I_S_BeginPrecaching;
    plugin_info_sound_funcs.pS_EndPrecaching = I_S_EndPrecaching;
    plugin_info_sound_funcs.pS_ExtraUpdate = I_S_ExtraUpdate;
    plugin_info_sound_funcs.pS_LocalSound = I_S_LocalSound;

    return &plugin_info;
}
