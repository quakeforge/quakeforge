/*
	snd_sdl.c

	(description)

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

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <SDL_audio.h>
#include <SDL_byteorder.h>
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/console.h"
#include "QF/qargs.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/plugin.h"

static dma_t the_shm;
static int  snd_inited;

extern int  desired_speed;
extern int  desired_bits;

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

void I_S_PaintChannels (int endtime);

static void
paint_audio (void *unused, Uint8 * stream, int len)
{
	int streamsamples;
	int sampleposbytes;
	int samplesbytes;

	if (shm) {
		streamsamples = len / (shm->samplebits / 8);
		sampleposbytes = shm->samplepos * (shm->samplebits / 8);
		samplesbytes = shm->samples * (shm->samplebits / 8);

		shm->samplepos += streamsamples;
		while (shm->samplepos >= shm->samples)
			shm->samplepos -= shm->samples;
		I_S_PaintChannels (soundtime + streamsamples);

		if (shm->samplepos + streamsamples <= shm->samples)
			memcpy (stream, shm->buffer + sampleposbytes, len);
		else {
			memcpy (stream, shm->buffer + sampleposbytes, samplesbytes - sampleposbytes);
			memcpy (stream + samplesbytes - sampleposbytes, shm->buffer, len - (samplesbytes - sampleposbytes));
		}
		soundtime += streamsamples;
	}
}

qboolean
SNDDMA_Init (void)
{
	SDL_AudioSpec desired, obtained;

	snd_inited = 0;

	/* Set up the desired format */
	desired.freq = desired_speed;
	switch (desired_bits) {
		case 8:
			desired.format = AUDIO_U8;
			break;
		case 16:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				desired.format = AUDIO_S16MSB;
			else
				desired.format = AUDIO_S16LSB;
			break;
		default:
			Con_Printf ("Unknown number of audio bits: %d\n", desired_bits);
			return 0;
	}
	desired.channels = 2;
	desired.samples = 1024;
	desired.callback = paint_audio;

	/* Open the audio device */
	if (SDL_OpenAudio (&desired, &obtained) < 0) {
		Con_Printf ("Couldn't open SDL audio: %s\n", SDL_GetError ());
		return 0;
	}

	/* Make sure we can support the audio format */
	switch (obtained.format) {
		case AUDIO_U8:
			/* Supported */
			break;
		case AUDIO_S16LSB:
		case AUDIO_S16MSB:
			if (((obtained.format == AUDIO_S16LSB) &&
				 (SDL_BYTEORDER == SDL_LIL_ENDIAN)) ||
				((obtained.format == AUDIO_S16MSB) &&
				 (SDL_BYTEORDER == SDL_BIG_ENDIAN))) {
				/* Supported */
				break;
			}
			/* Unsupported, fall through */ ;
		default:
			/* Not supported -- force SDL to do our bidding */
			SDL_CloseAudio ();
			if (SDL_OpenAudio (&desired, NULL) < 0) {
				Con_Printf ("Couldn't open SDL audio: %s\n", SDL_GetError ());
				return 0;
			}
			memcpy (&obtained, &desired, sizeof (desired));
			break;
	}
	SDL_LockAudio();
	SDL_PauseAudio (0);

	/* Fill the audio DMA information block */
	shm = &the_shm;
	shm->splitbuffer = 0;
	shm->samplebits = (obtained.format & 0xFF);
	shm->speed = obtained.freq;
	shm->channels = obtained.channels;
	shm->samples = obtained.samples * 16;
	shm->samplepos = 0;
	shm->submission_chunk = 1;
	shm->buffer = calloc(shm->samples * (shm->samplebits / 8), 1);
	if (!shm->buffer)
	{
		Sys_Error ("Failed to allocate buffer for sound!\n");
	}

	snd_inited = 1;
	return 1;
}

int
SNDDMA_GetDMAPos (void)
{
	return shm->samplepos;
}

void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		SDL_PauseAudio (1);
		SDL_UnlockAudio ();
		SDL_CloseAudio ();
		snd_inited = 0;
		shm = NULL;
	}
}

/*

	SNDDMA_Submit

	Send sound to device if buffer isn't really the dma buffer

*/
void
SNDDMA_Submit (void)
{
	SDL_UnlockAudio();
	SDL_LockAudio();
}

plugin_t *
PluginInfo (void) {
    plugin_info.type = qfp_sound;
    plugin_info.api_version = QFPLUGIN_VERSION;
    plugin_info.plugin_version = "0.1";
    plugin_info.description = "SDL digital output";
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
