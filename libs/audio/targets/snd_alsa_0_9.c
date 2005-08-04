/*
	snd_alsa_0_9.c

	Support for the obsolete ALSA 0.9 sound driver

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

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#include <stdio.h>
#include <dlfcn.h>
#include <alsa/asoundlib.h>

#include "QF/cvar.h"
#include "QF/plugin.h"
#include "QF/qargs.h"
#include "QF/sound.h"
#include "QF/sys.h"

static int			snd_inited;
static int			snd_blocked = 0;
static volatile dma_t sn;
static size_t		buffer_size;

static void		   *alsa_handle;
static const char  *pcmname = NULL;
static snd_pcm_t   *pcm;

static plugin_t			  plugin_info;
static plugin_data_t	  plugin_info_data;
static plugin_funcs_t	  plugin_info_funcs;
static general_data_t	  plugin_info_general_data;
static general_funcs_t	  plugin_info_general_funcs;
static snd_output_data_t  plugin_info_snd_output_data;
static snd_output_funcs_t plugin_info_snd_output_funcs;

static cvar_t	   *snd_bits;
static cvar_t      *snd_device;
static cvar_t      *snd_rate;
static cvar_t      *snd_stereo;

#define QF_ALSA_NEED(ret, func, params) \
static ret (*qf##func) params;
#include "alsa_funcs_list.h"
#undef QF_ALSA_NEED

static qboolean
load_libasound (void)
{
	if (!(alsa_handle = dlopen ("libasound.so.2", RTLD_GLOBAL | RTLD_NOW))) {
		Sys_Printf ("Couldn't load libasound.so.2: %s\n", dlerror ());
		return false;
	}
#define QF_ALSA_NEED(ret, func, params) \
	if (!(qf##func = dlsym (alsa_handle, #func))) { \
		Sys_Printf ("Couldn't load ALSA function %s\n", #func); \
		dlclose (alsa_handle); \
		alsa_handle = 0; \
		return false; \
	}
#include "alsa_funcs_list.h"
#undef QF_ALSA_NEED
	return true;
}

#define snd_pcm_hw_params_sizeof qfsnd_pcm_hw_params_sizeof
#define snd_pcm_sw_params_sizeof qfsnd_pcm_sw_params_sizeof

static void
SNDDMA_Init_Cvars (void)
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

static volatile dma_t *
SNDDMA_Init (void)
{
	int					 err, frag_size;
	int					 bps = -1, rate = -1, stereo = -1;
	snd_pcm_hw_params_t	*hw;
	snd_pcm_sw_params_t	*sw;

	if (!load_libasound ())
		return false;

	snd_pcm_hw_params_alloca (&hw);
	snd_pcm_sw_params_alloca (&sw);

	if (snd_device->string[0])
		pcmname = snd_device->string;
	if (snd_bits->int_val) {
		bps = snd_bits->int_val;
		if (bps != 16 && bps != 8) {
			Sys_Printf ("Error: invalid sample bits: %d\n", bps);
			return 0;
		}
	}
	if (snd_rate->int_val) {
		rate = snd_rate->int_val;
		if (rate != 44100 && rate != 22050 && rate != 11025) {
			Sys_Printf ("Error: invalid sample rate: %d\n", rate);
			return 0;
		}
	}
	stereo = snd_stereo->int_val;

	if (!pcmname)
		pcmname = "default";
	if ((err = qfsnd_pcm_open (&pcm, pcmname, SND_PCM_STREAM_PLAYBACK,
                               SND_PCM_NONBLOCK)) < 0) {
        Sys_Printf ("Error: audio open error: %s\n", qfsnd_strerror (err));
        return 0;
    }
	Sys_Printf ("Using PCM %s.\n", pcmname);
	qfsnd_pcm_hw_params_any (pcm, hw);

	if (qfsnd_pcm_hw_params_set_access (pcm, hw,
										SND_PCM_ACCESS_MMAP_INTERLEAVED) < 0) {
		Sys_Printf ("ALSA: interleaved is not supported\n");
		goto error;
	}

	switch (bps) {
		case -1:
			if (qfsnd_pcm_hw_params_set_format (pcm, hw, SND_PCM_FORMAT_S16_LE)
				>= 0) {
				bps = 16;
			} else if (qfsnd_pcm_hw_params_set_format (pcm, hw,
													   SND_PCM_FORMAT_U8)
					   >= 0) {
				bps = 8;
			} else {
				Sys_Printf ("ALSA: no useable formats\n");
				goto error;
			}
			break;
		case 8:
		case 16:
			if (qfsnd_pcm_hw_params_set_format (pcm, hw,
												bps == 8 ? SND_PCM_FORMAT_U8 :
												SND_PCM_FORMAT_S16) >= 0) {
				break;
			}
			// Fall through
		default:
			Sys_Printf ("ALSA: desired format not supported\n");
			goto error;
	}

	switch (stereo) {
		case -1:
			if (qfsnd_pcm_hw_params_set_channels (pcm, hw, 2) >= 0) {
				stereo = 1;
			} else if (qfsnd_pcm_hw_params_set_channels (pcm, hw, 1) >= 0) {
				stereo = 0;
			} else {
				Sys_Printf ("ALSA: no useable channels\n");
				goto error;
			}
			break;
		case 0:
		case 1:
			if (qfsnd_pcm_hw_params_set_channels (pcm, hw, stereo ? 2 : 1)
				>= 0)
				break;
			// Fall through
		default:
			Sys_Printf ("ALSA: desired channels not supported\n");
			goto error;
	}

	switch (rate) {
		case -1:
		case 0:
			rate = 44100;
			if (qfsnd_pcm_hw_params_set_rate_near (pcm, hw, rate, 0) >= 0) {
				frag_size = 32 * bps;
			} else {
				rate = 22050;
				if (qfsnd_pcm_hw_params_set_rate_near (pcm, hw, rate, 0)
					>= 0) {
					frag_size = 16 * bps;
				} else {
					rate = 11025;
					if (qfsnd_pcm_hw_params_set_rate_near (pcm, hw, rate, 0)
						>= 0) {
						frag_size = 8 * bps;
					} else {
						Sys_Printf ("ALSA: no useable rates\n");
						goto error;
					}
				}
			}
			break;
		case 11025:
		case 22050:
		case 44100:
			if (0 <= qfsnd_pcm_hw_params_set_rate_near (pcm, hw, rate, 0)) {
				frag_size = 8 * bps * rate / 11025;
				break;
			}
			// Fall through
		default:
			Sys_Printf ("ALSA: desired rate not supported\n");
			goto error;
	}

	qfsnd_pcm_hw_params_set_period_size_near (pcm, hw, frag_size, 0);

	err = qfsnd_pcm_hw_params (pcm, hw);
	if (0 > err) {
		Sys_Printf ("ALSA: unable to install hw params\n");
		goto error;
	}

	qfsnd_pcm_sw_params_current (pcm, sw);
	qfsnd_pcm_sw_params_set_start_threshold (pcm, sw, ~0U);
	qfsnd_pcm_sw_params_set_stop_threshold (pcm, sw, ~0U);

	err = qfsnd_pcm_sw_params (pcm, sw);
	if (0 > err) {
		Sys_Printf ("ALSA: unable to install sw params: %s\n",
					qfsnd_strerror (err));
		goto error;
	}

	memset ((dma_t *) &sn, 0, sizeof (sn));
	sn.splitbuffer = 0;
	sn.channels = stereo + 1;
	sn.submission_chunk = qfsnd_pcm_hw_params_get_period_size (hw, 0);
										// don't mix less than this
	sn.samplepos = 0;					// in mono samples
	sn.samplebits = bps;

	buffer_size = qfsnd_pcm_hw_params_get_buffer_size (hw);

	sn.samples = buffer_size * sn.channels;		// mono samples in buffer
	sn.speed = rate;
	SNDDMA_GetDMAPos ();						// XXX sets sn.buffer
	Sys_Printf ("%5d stereo\n", sn.channels - 1);
	Sys_Printf ("%5d samples\n", sn.samples);
	Sys_Printf ("%5d samplepos\n", sn.samplepos);
	Sys_Printf ("%5d samplebits\n", sn.samplebits);
	Sys_Printf ("%5d submission_chunk\n", sn.submission_chunk);
	Sys_Printf ("%5d speed\n", sn.speed);
	Sys_Printf ("0x%x dma buffer\n", (int) sn.buffer);

	snd_inited = 1;
	return &sn;
error:
	qfsnd_pcm_close (pcm);
	return 0;
}

static int
SNDDMA_GetDMAPos (void)
{
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t offset;
	snd_pcm_uframes_t nframes = sn.samples/sn.channels;

	if (!snd_inited)
		return 0;

	qfsnd_pcm_avail_update (pcm);
	qfsnd_pcm_mmap_begin (pcm, &areas, &offset, &nframes);
	offset *= sn.channels;
	nframes *= sn.channels;
	sn.samplepos = offset;
	sn.buffer = areas->addr;	//XXX FIXME there's an area per channel
	return sn.samplepos;
}

static void
SNDDMA_Shutdown (void)
{
	if (snd_inited) {
		qfsnd_pcm_close (pcm);
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
	int			state;
	int			count = (*plugin_info_snd_output_data.paintedtime -
						 *plugin_info_snd_output_data.soundtime);
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t nframes;
	snd_pcm_uframes_t offset;

	if (snd_blocked)
		return;

	nframes = count / sn.channels;

	qfsnd_pcm_avail_update (pcm);
	qfsnd_pcm_mmap_begin (pcm, &areas, &offset, &nframes);

	state = qfsnd_pcm_state (pcm);

	switch (state) {
		case SND_PCM_STATE_PREPARED:
			qfsnd_pcm_mmap_commit (pcm, offset, nframes);
			qfsnd_pcm_start (pcm);
			break;
		case SND_PCM_STATE_RUNNING:
			qfsnd_pcm_mmap_commit (pcm, offset, nframes);
			break;
		default:
			break;
	}
}

static void
SNDDMA_BlockSound (void)
{
	if (snd_inited && ++snd_blocked == 1)
		qfsnd_pcm_pause (pcm, 1);
}

static void
SNDDMA_UnblockSound (void)
{
	if (!snd_inited || !snd_blocked)
		return;
	if (!--snd_blocked)
		qfsnd_pcm_pause (pcm, 0);
}

PLUGIN_INFO(snd_output, alsa0_9)
{
	plugin_info.type = qfp_snd_output;
	plugin_info.api_version = QFPLUGIN_VERSION;
	plugin_info.plugin_version = "0.1";
	plugin_info.description = "ALSA 0.9.x digital output";
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
