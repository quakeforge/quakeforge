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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <SDL.h>
#include <SDL_audio.h>
#include <SDL_byteorder.h>
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "snd_render.h"

static dma_t sn;
static int  snd_inited;
static int snd_blocked = 0;

static int desired_speed = 11025;
static int desired_bits = 16;

static plugin_t           plugin_info;
static plugin_data_t      plugin_info_data;
static plugin_funcs_t     plugin_info_funcs;
static general_data_t     plugin_info_general_data;
static general_funcs_t    plugin_info_general_funcs;
static snd_output_data_t       plugin_info_snd_output_data;
static snd_output_funcs_t      plugin_info_snd_output_funcs;


static void
paint_audio (void *unused, Uint8 * stream, int len)
{
	int sampleposbytes, samplesbytes, streamsamples;

	streamsamples = len / (sn.samplebits / 8);
	sampleposbytes = sn.samplepos * (sn.samplebits / 8);
	samplesbytes = sn.samples * (sn.samplebits / 8);

	sn.samplepos += streamsamples;
	while (sn.samplepos >= sn.samples)
		sn.samplepos -= sn.samples;

	if (sn.samplepos + streamsamples <= sn.samples)
		memcpy (stream, sn.buffer + sampleposbytes, len);
	else {
		memcpy (stream, sn.buffer + sampleposbytes, samplesbytes -
				sampleposbytes);
		memcpy (stream + samplesbytes - sampleposbytes, sn.buffer, len -
				(samplesbytes - sampleposbytes));
	}
	*plugin_info_snd_output_data.soundtime += streamsamples;
}

static void
SNDDMA_Init_Cvars (void)
{
}

static volatile dma_t *
SNDDMA_Init (void)
{
	SDL_AudioSpec desired, obtained;

	snd_inited = 0;

	if (SDL_Init (SDL_INIT_AUDIO) < 0) {
		Sys_Printf ("Couldn't initialize SDL AUDIO: %s\n", SDL_GetError ());
		return 0;
	};

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
			Sys_Printf ("Unknown number of audio bits: %d\n", desired_bits);
			return 0;
	}
	desired.channels = 2;
	desired.samples = 1024;
	desired.callback = paint_audio;

	/* Open the audio device */
	if (SDL_OpenAudio (&desired, &obtained) < 0) {
		Sys_Printf ("Couldn't open SDL audio: %s\n", SDL_GetError ());
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
				Sys_Printf ("Couldn't open SDL audio: %s\n", SDL_GetError ());
				return 0;
			}
			memcpy (&obtained, &desired, sizeof (desired));
			break;
	}
	SDL_LockAudio();
	SDL_PauseAudio (0);

	/* Fill the audio DMA information block */
	sn.samplebits = (obtained.format & 0xFF);
	sn.speed = obtained.freq;
	sn.channels = obtained.channels;
	sn.samples = obtained.samples * 16;
	sn.samplepos = 0;
	sn.submission_chunk = 1;
	sn.buffer = calloc(sn.samples * (sn.samplebits / 8), 1);
	if (!sn.buffer)
	{
		Sys_Error ("Failed to allocate buffer for sound!");
	}

	snd_inited = 1;
	return &sn;
}

static int
SNDDMA_GetDMAPos (void)
{
	return sn.samplepos;
}

static void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		SDL_PauseAudio (1);
		SDL_UnlockAudio ();
		SDL_CloseAudio ();
		snd_inited = 0;
	}
}

/*
	SNDDMA_Submit

	Send sound to device if buffer isn't really the dma buffer
*/
static void
SNDDMA_Submit (void)
{
	if (snd_blocked)
		return;

	SDL_UnlockAudio();
	SDL_LockAudio();
}

static void
SNDDMA_BlockSound (void)
{
	++snd_blocked;
}

static void
SNDDMA_UnblockSound (void)
{
	if (!snd_blocked)
		return;
	--snd_blocked;
}

PLUGIN_INFO(snd_output, sdl)
{
	plugin_info.type = qfp_snd_output;
	plugin_info.api_version = QFPLUGIN_VERSION;
	plugin_info.plugin_version = "0.1";
	plugin_info.description = "SDL digital output";
	plugin_info.copyright = "Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001  contributors of the QuakeForge "
		"project\n"
		"Please see the file \"AUTHORS\" for a list of contributors";
	plugin_info.functions = &plugin_info_funcs;
	plugin_info.data = &plugin_info_data;

	plugin_info_data.general = &plugin_info_general_data;
	plugin_info_data.input = NULL;
	plugin_info_data.snd_output = &plugin_info_snd_output_data;

	plugin_info_funcs.general = &plugin_info_general_funcs;
	plugin_info_funcs.input = NULL;
	plugin_info_funcs.snd_output = &plugin_info_snd_output_funcs;

	plugin_info_general_funcs.p_Init = SNDDMA_Init_Cvars;
	plugin_info_general_funcs.p_Shutdown = NULL;
	plugin_info_snd_output_funcs.pS_O_Init = SNDDMA_Init;
	plugin_info_snd_output_funcs.pS_O_Shutdown = SNDDMA_Shutdown;
	plugin_info_snd_output_funcs.pS_O_GetDMAPos = SNDDMA_GetDMAPos;
	plugin_info_snd_output_funcs.pS_O_Submit = SNDDMA_Submit;
	plugin_info_snd_output_funcs.pS_O_BlockSound = SNDDMA_BlockSound;
	plugin_info_snd_output_funcs.pS_O_UnblockSound = SNDDMA_UnblockSound;

	return &plugin_info;
}
