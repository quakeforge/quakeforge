/*
	snd_alsa_0_9.c

	Support for ALSA 0.9 sound driver (cvs development version)

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

#include <stdio.h>
#include <sys/asoundlib.h>

#include "QF/console.h"
#include "QF/cvar.h"
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/sound.h"

static int  snd_inited;

static snd_pcm_t *pcm;
static const char *pcmname = NULL;
static size_t      buffer_size;
static int snd_blocked = 0;
static volatile dma_t sn;

static cvar_t     *snd_stereo;
static cvar_t     *snd_rate;
static cvar_t     *snd_device;
static cvar_t     *snd_bits;

static plugin_t           plugin_info;
static plugin_data_t      plugin_info_data;
static plugin_funcs_t     plugin_info_funcs;
static general_data_t     plugin_info_general_data;
static general_funcs_t    plugin_info_general_funcs;
static snd_output_data_t       plugin_info_snd_output_data;
static snd_output_funcs_t      plugin_info_snd_output_funcs;

void
static SNDDMA_Init_Cvars (void)
{
	snd_stereo = Cvar_Get ("snd_stereo", "1", CVAR_ROM, NULL,
						   "sound stereo output");
	snd_rate = Cvar_Get ("snd_rate", "0", CVAR_ROM, NULL,
						 "sound playback rate. 0 is system default");
	snd_device = Cvar_Get ("snd_device", "", CVAR_ROM, NULL,
						 "sound device. \"\" is system default");
	snd_bits = Cvar_Get ("snd_bits", "0", CVAR_ROM, NULL,
						 "sound sample depth. 0 is system default");
}

static int SNDDMA_GetDMAPos (void);

static qboolean
SNDDMA_Init (void)
{
	int         err;
	int         rate = -1, bps = -1, stereo = -1, frag_size;
	snd_pcm_hw_params_t *hw;
	snd_pcm_sw_params_t *sw;

	snd_pcm_hw_params_alloca (&hw);
	snd_pcm_sw_params_alloca (&sw);

	if (snd_device->string[0])
		pcmname = snd_device->string;
	if (snd_bits->int_val) {
		bps = snd_bits->int_val;
		if (bps != 16 && bps != 8) {
			Con_Printf ("Error: invalid sample bits: %d\n", bps);
			return 0;
		}
	}
	if (snd_rate->int_val) {
		rate = snd_rate->int_val;
		if (rate != 44100 && rate != 22050 && rate != 11025) {
			Con_Printf ("Error: invalid sample rate: %d\n", rate);
			return 0;
		}
	}
	stereo = snd_stereo->int_val;
	if (!pcmname)
		pcmname = "plug:0,0";
	if ((err = snd_pcm_open (&pcm, pcmname,
							 SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
		Con_Printf ("Error: audio open error: %s\n", snd_strerror (err));
		return 0;
	}

	Con_Printf ("Using PCM %s.\n", pcmname);
	snd_pcm_hw_params_any (pcm, hw);


	switch (rate) {
		case -1:
			if (snd_pcm_hw_params_set_rate_near (pcm, hw, 44100, 0) >= 0) {
				frag_size = 256;		/* assuming stereo 8 bit */
				rate = 44100;
			} else if (snd_pcm_hw_params_set_rate_near (pcm, hw, 22050, 0) >=
					   0) {
				frag_size = 128;		/* assuming stereo 8 bit */
				rate = 22050;
			} else if (snd_pcm_hw_params_set_rate_near (pcm, hw, 11025, 0) >=
					   0) {
				frag_size = 64;			/* assuming stereo 8 bit */
				rate = 11025;
			} else {
				Con_Printf ("ALSA: no useable rates\n");
				goto error;
			}
			break;
		case 11025:
		case 22050:
		case 44100:
			if (snd_pcm_hw_params_set_rate_near (pcm, hw, rate, 0) >= 0) {
				frag_size = 64 * rate / 11025;	/* assuming stereo 8 bit */
				break;
			}
			/* Fall through */
		default:
			Con_Printf ("ALSA: desired rate not supported\n");
			goto error;
	}

	switch (bps) {
		case -1:
			if (snd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_S16_LE)
				>= 0) {
				bps = 16;
			} else if (snd_pcm_hw_params_set_format (pcm, hw,
													 SND_PCM_FORMAT_U8)
					   >= 0) {
				bps = 8;
			} else {
				Con_Printf ("ALSA: no useable formats\n");
				goto error;
			}
			break;
		case 8:
		case 16:
			if (snd_pcm_hw_params_set_format (pcm, hw,
											  bps == 8 ? SND_PCM_FORMAT_U8 :
											  SND_PCM_FORMAT_S16) >= 0) {
				break;
			}
			/* Fall through */
		default:
			Con_Printf ("ALSA: desired format not supported\n");
			goto error;
	}

	if (snd_pcm_hw_params_set_access (pcm, hw,
									  SND_PCM_ACCESS_MMAP_INTERLEAVED) < 0) {
		Con_Printf ("ALSA: interleaved is not supported\n");
		goto error;
	}

	switch (stereo) {
		case -1:
			if (snd_pcm_hw_params_set_channels (pcm, hw, 2) >= 0) {
				stereo = 1;
			} else if (snd_pcm_hw_params_set_channels (pcm, hw, 1) >= 0) {
				stereo = 0;
			} else {
				Con_Printf ("ALSA: no useable channels\n");
				goto error;
			}
			break;
		case 0:
		case 1:
			if (snd_pcm_hw_params_set_channels (pcm, hw, stereo ? 2 : 1) >= 0)
				break;
			/* Fall through */
		default:
			Con_Printf ("ALSA: desired channels not supported\n");
			goto error;
	}

	snd_pcm_hw_params_set_period_size_near (pcm, hw, frag_size, 0);

	err = snd_pcm_hw_params (pcm, hw);
	if (err < 0) {
		Con_Printf ("ALSA: unable to install hw params\n");
		goto error;
	}

	snd_pcm_sw_params_current (pcm, sw);
	snd_pcm_sw_params_set_start_threshold (pcm, sw, ~0U);
	snd_pcm_sw_params_set_stop_threshold (pcm, sw, ~0U);

	err = snd_pcm_sw_params (pcm, sw);
	if (err < 0) {
		Con_Printf ("ALSA: unable to install sw params\n");
		goto error;
	}

	shm = &sn;
	memset ((dma_t *) shm, 0, sizeof (*shm));
	shm->splitbuffer = 0;
	shm->channels = stereo + 1;
	shm->submission_chunk = snd_pcm_hw_params_get_period_size (hw, 0); // don't
										// mix less than this #
	shm->samplepos = 0;					// in mono samples
	shm->samplebits = bps;
	buffer_size = snd_pcm_hw_params_get_buffer_size (hw);
	shm->samples = buffer_size * shm->channels;	// mono samples in buffer
	shm->speed = rate;
	SNDDMA_GetDMAPos ();//XXX sets shm->buffer
	Con_Printf ("%5d stereo\n", shm->channels - 1);
	Con_Printf ("%5d samples\n", shm->samples);
	Con_Printf ("%5d samplepos\n", shm->samplepos);
	Con_Printf ("%5d samplebits\n", shm->samplebits);
	Con_Printf ("%5d submission_chunk\n", shm->submission_chunk);
	Con_Printf ("%5d speed\n", shm->speed);
	Con_Printf ("0x%x dma buffer\n", (int) shm->buffer);
	Con_Printf ("%5d total_channels\n", total_channels);

	snd_inited = 1;
	return 1;
error:
	snd_pcm_close (pcm);
	return 0;
}

static int
SNDDMA_GetDMAPos (void)
{
	snd_pcm_uframes_t offset;
	snd_pcm_uframes_t nframes = shm->samples/shm->channels;
	const snd_pcm_channel_area_t *areas;

	if (!snd_inited)
		return 0;

	snd_pcm_mmap_begin (pcm, &areas, &offset, &nframes);
	offset *= shm->channels;
	nframes *= shm->channels;
	shm->samplepos = offset;
	shm->buffer = areas->addr;//XXX FIXME there's an area per channel
	return shm->samplepos;
}

static void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		snd_pcm_close (pcm);
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
	snd_pcm_uframes_t offset;
	snd_pcm_uframes_t nframes;
	const snd_pcm_channel_area_t *areas;
	int         state;
	int         count = *plugin_info_snd_output_data.paintedtime
					- *plugin_info_snd_output_data.soundtime;

	if (snd_blocked)
		return;

	nframes = count / shm->channels;

	snd_pcm_mmap_begin (pcm, &areas, &offset, &nframes);

	state = snd_pcm_state (pcm);

	switch (state) {
		case SND_PCM_STATE_PREPARED:
			snd_pcm_mmap_commit (pcm, offset, nframes);
			snd_pcm_start (pcm);
			break;
		case SND_PCM_STATE_RUNNING:
			snd_pcm_mmap_commit (pcm, offset, nframes);
			break;
		default:
			break;
	}
}

static void
SNDDMA_BlockSound (void)
{
	if (++snd_blocked == 1)
		snd_pcm_pause (pcm, 1);
}

static void
SNDDMA_UnblockSound (void)
{
	if (!snd_blocked)
		return;
	if (!--snd_blocked)
		snd_pcm_pause (pcm, 0);
}

plugin_t *
snd_output_alsa0_9_PluginInfo (void)
{
    plugin_info.type = qfp_snd_output;
    plugin_info.api_version = QFPLUGIN_VERSION;
    plugin_info.plugin_version = "0.1";
    plugin_info.description = "ALSA 0.9.x digital output";
    plugin_info.copyright = "Copyright (C) 1996-1997 id Software, Inc.\n"
        "Copyright (C) 1999,2000,2001  contributors of the QuakeForge"
		" project\n"
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
