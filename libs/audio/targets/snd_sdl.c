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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <SDL.h>
#include <SDL_audio.h>
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "snd_internal.h"

static int snd_inited;
static int snd_blocked;

static unsigned shm_buflen;
static unsigned char *shm_buf;
static unsigned shm_rpos;

static unsigned wpos;

static int snd_bits;
static cvar_t snd_bits_cvar = {
	.name = "snd_bits",
	.description =
		"sound sample depth. 0 is system default",
	.default_value = "0",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &snd_bits },
};
static int snd_rate;
static cvar_t snd_rate_cvar = {
	.name = "snd_rate",
	.description =
		"sound playback rate. 0 is system default",
	.default_value = "0",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &snd_rate },
};
static int snd_stereo;
static cvar_t snd_stereo_cvar = {
	.name = "snd_stereo",
	.description =
		"sound stereo output",
	.default_value = "1",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &snd_stereo },
};

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
	while (shm_rpos + len > shm_buflen) {
		memcpy (stream, shm_buf + shm_rpos, shm_buflen - shm_rpos);
		stream += shm_buflen - shm_rpos;
		len -= shm_buflen - shm_rpos;
		shm_rpos = 0;
	}
	if (len) {
		memcpy (stream, shm_buf + shm_rpos, len);
		shm_rpos += len;
	}
}

static void
SNDDMA_Init_Cvars (void)
{
	Cvar_Register (&snd_stereo_cvar, 0, 0);
	Cvar_Register (&snd_rate_cvar, 0, 0);
	Cvar_Register (&snd_bits_cvar, 0, 0);
}

static int
SNDDMA_Init (snd_t *snd)
{
	SDL_AudioSpec desired, obtained;

	if (snd_inited)
		return 1;

	if (SDL_Init (SDL_INIT_AUDIO) < 0) {
		Sys_Printf ("Couldn't initialize SDL AUDIO: %s\n", SDL_GetError ());
		return 0;
	};

	/* Set up the desired format */
	desired.freq = 22050;
	if (snd_rate)
		desired.freq = snd_rate;
	switch (snd_bits) {
		case 8:
			desired.format = AUDIO_U8;
			break;
		case 0:		// default is 16 bits per sample
		case 16:
			if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
				desired.format = AUDIO_S16MSB;
			else
				desired.format = AUDIO_S16LSB;
			break;
		default:
			Sys_Printf ("Unknown number of audio bits: %d\n",
						snd_bits);
			return 0;
	}
	desired.channels = snd_stereo ? 2 : 1;
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

	/* Fill the audio DMA information block */
	snd->samplebits = (obtained.format & 0xFF);
	snd->speed = obtained.freq;
	snd->channels = obtained.channels;
	snd->frames = obtained.samples * 8;	// 8 chunks in the buffer
	snd->framepos = 0;
	snd->submission_chunk = 1;

	shm_buflen = snd->frames * snd->channels * (snd->samplebits / 8);

	snd->buffer = calloc (shm_buflen, 1);
	if (!snd->buffer)
		Sys_Error ("Failed to allocate buffer for sound!");

	shm_buf = calloc (shm_buflen, 1);
	if (!shm_buf)
		Sys_Error ("Failed to allocate buffer for sound!");

	shm_rpos = wpos = 0;

	if (!snd_blocked)
		SDL_PauseAudio (0);

	snd_inited = 1;

	return 1;
}

static int
SNDDMA_GetDMAPos (snd_t *snd)
{
	if (!snd_inited)
		return 0;

	SDL_LockAudio ();
	snd->framepos = shm_rpos / (snd->channels * (snd->samplebits / 8));
	SDL_UnlockAudio ();

	return snd->framepos;
}

static void
SNDDMA_shutdown (snd_t *snd)
{
	if (snd_inited) {
		SDL_CloseAudio ();
		free (snd->buffer);
		free (shm_buf);
		snd_inited = 0;
	}
}

/*
	SNDDMA_Submit

	Send sound to device if buffer isn't really the dma buffer
*/
static void
SNDDMA_Submit (snd_t *snd)
{
	static unsigned old_paintedtime;
	unsigned len;

	if (!snd_inited || snd_blocked)
		return;

	SDL_LockAudio ();

	if (*plugin_info_snd_output_data.paintedtime < old_paintedtime)
		old_paintedtime = 0;

	len = (*plugin_info_snd_output_data.paintedtime - old_paintedtime) *
			snd->channels * (snd->samplebits / 8);

	old_paintedtime = *plugin_info_snd_output_data.paintedtime;

	while (wpos + len > shm_buflen) {
		memcpy (shm_buf + wpos, snd->buffer + wpos, shm_buflen - wpos);
		len -= shm_buflen - wpos;
		wpos = 0;
	}
	if (len) {
		memcpy (shm_buf + wpos, snd->buffer + wpos, len);
		wpos += len;
	}

	SDL_UnlockAudio ();
}

static void
SNDDMA_BlockSound (snd_t *snd)
{
	if (!snd_inited)
		return;

	if (++snd_blocked == 1)
		SDL_PauseAudio (1);
}

static void
SNDDMA_UnblockSound (snd_t *snd)
{
	if (!snd_inited || !snd_blocked)
		return;

	if (!--snd_blocked)
		SDL_PauseAudio (0);
}

PLUGIN_INFO(snd_output, sdl)
{
	plugin_info.type = qfp_snd_output;
	plugin_info.api_version = QFPLUGIN_VERSION;
	plugin_info.plugin_version = "0.1";
	plugin_info.description = "SDL digital output";
	plugin_info.copyright = "Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001,2012  contributors of the QuakeForge "
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

	plugin_info_general_funcs.init = SNDDMA_Init_Cvars;
	plugin_info_general_funcs.shutdown = NULL;
	plugin_info_snd_output_funcs.init = SNDDMA_Init;
	plugin_info_snd_output_funcs.shutdown = SNDDMA_shutdown;
	plugin_info_snd_output_funcs.get_dma_pos = SNDDMA_GetDMAPos;
	plugin_info_snd_output_funcs.submit = SNDDMA_Submit;
	plugin_info_snd_output_funcs.block_sound = SNDDMA_BlockSound;
	plugin_info_snd_output_funcs.unblock_sound = SNDDMA_UnblockSound;

	return &plugin_info;
}
