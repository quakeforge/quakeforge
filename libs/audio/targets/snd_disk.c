/*
	snd_disk.c

	write sound to a disk file

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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>

#include "QF/console.h"
#include "QF/sound.h"
#include "QF/qargs.h"
#include "QF/plugin.h"

static int  snd_inited;
VFile      *snd_file;

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

qboolean
SNDDMA_Init (void)
{
	shm = &sn;
	memset ((dma_t *) shm, 0, sizeof (*shm));
	shm->splitbuffer = 0;
	shm->channels = 2;
	shm->submission_chunk = 1;			// don't mix less than this #
	shm->samplepos = 0;					// in mono samples
	shm->samplebits = 16;
	shm->samples = 16384;				// mono samples in buffer
	shm->speed = 44100;
	shm->buffer = malloc (shm->samples * shm->channels * shm->samplebits / 8);

	Con_Printf ("%5d stereo\n", shm->channels - 1);
	Con_Printf ("%5d samples\n", shm->samples);
	Con_Printf ("%5d samplepos\n", shm->samplepos);
	Con_Printf ("%5d samplebits\n", shm->samplebits);
	Con_Printf ("%5d submission_chunk\n", shm->submission_chunk);
	Con_Printf ("%5d speed\n", shm->speed);
	Con_Printf ("0x%x dma buffer\n", (int) shm->buffer);
	Con_Printf ("%5d total_channels\n", total_channels);

	if (!(snd_file = Qopen ("qf.raw", "wb")))
		return 0;

	snd_inited = 1;
	return 1;
}

int
SNDDMA_GetDMAPos (void)
{
	shm->samplepos = 0;
	return shm->samplepos;
}

void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		Qclose (snd_file);
		snd_file = 0;
		free (shm->buffer);
		snd_inited = 0;
	}
}

/*
	SNDDMA_Submit

	Send sound to device if buffer isn't really the dma buffer
*/
void
SNDDMA_Submit (void)
{
	int         count = (paintedtime - soundtime) * shm->samplebits / 8;

	Qwrite (snd_file, shm->buffer, count);
}

plugin_t *
PluginInfo (void) {
    plugin_info.type = qfp_sound;
    plugin_info.api_version = QFPLUGIN_VERSION;
    plugin_info.plugin_version = "0.1";
    plugin_info.description = "disk output";
    plugin_info.copyright = "Copyright (C) 1996-1997 id Software, Inc.\n"
        "Copyright (C) 1999,2000,2001  contributors of the QuakeForge project\n"        "Please see the file \"AUTHORS\" for a list of contributors";
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
