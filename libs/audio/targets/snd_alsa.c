/*
	snd_alsa.c

	Support for the ALSA 1.0.1 sound driver

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

#include <stdio.h>
#include <dlfcn.h>
#include <alsa/asoundlib.h>

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "snd_internal.h"

static int			snd_inited;
static int			snd_blocked = 0;
static volatile dma_t sn;
static snd_pcm_uframes_t buffer_size;

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

static cvar_t      *snd_bits;
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

static __attribute__((const)) snd_pcm_uframes_t
round_buffer_size (snd_pcm_uframes_t sz)
{
	snd_pcm_uframes_t mask = ~0;

	while (sz & mask) {
		sz &= mask;
		mask <<= 1;
	}
	return sz;
}

static inline int
clamp_16 (int val)
{
	if (val > 0x7fff)
		val = 0x7fff;
	else if (val < -0x8000)
		val = -0x8000;
	return val;
}

static inline int
clamp_8 (int val)
{
	if (val > 0x7f)
		val = 0x7f;
	else if (val < -0x80)
		val = -0x80;
	return val;
}

static void
SNDDMA_ni_xfer (portable_samplepair_t *paintbuffer, int count, float volume)
{
	const snd_pcm_channel_area_t *areas;
	int         out_idx, out_max;
	float      *p;

	areas = sn.xfer_data;

	p = (float *) paintbuffer;
	out_max = sn.frames - 1;
	out_idx = *plugin_info_snd_output_data.paintedtime;
	while (out_idx > out_max)
		out_idx -= out_max + 1;

	if (sn.samplebits == 16) {
		short      *out_0 = (short *) areas[0].addr;
		short      *out_1 = (short *) areas[1].addr;

		if (sn.channels == 2) {
			while (count--) {
				out_0[out_idx] = clamp_16 ((*p++ * volume) * 0x8000);
				out_1[out_idx] = clamp_16 ((*p++ * volume) * 0x8000);
				if (out_idx++ > out_max)
					out_idx = 0;
			}
		} else {
			while (count--) {
				out_0[out_idx] = clamp_16 ((*p++ * volume) * 0x8000);
				p++;		// skip right channel
				if (out_idx++ > out_max)
					out_idx = 0;
			}
		}
	} else if (sn.samplebits == 8) {
		byte       *out_0 = (byte *) areas[0].addr;
		byte       *out_1 = (byte *) areas[1].addr;

		if (sn.channels == 2) {
			while (count--) {
				out_0[out_idx] = clamp_8 ((*p++ * volume) * 0x80);
				out_1[out_idx] = clamp_8 ((*p++ * volume) * 0x80);
				if (out_idx++ > out_max)
					out_idx = 0;
			}
		} else {
			while (count--) {
				out_0[out_idx] = clamp_8 ((*p++ * volume) * 0x8000);
				p++;		// skip right channel
				if (out_idx++ > out_max)
					out_idx = 0;
			}
		}
	}
}

static volatile dma_t *
SNDDMA_Init (void)
{
	int					 err;
	int					 bps = -1, stereo = -1;
	unsigned int		 rate = 0;
	snd_pcm_hw_params_t	*hw;
	snd_pcm_hw_params_t	**_hw = &hw;
	snd_pcm_sw_params_t	*sw;
	snd_pcm_sw_params_t	**_sw = &sw;
	snd_pcm_uframes_t	 frag_size;

	if (!load_libasound ())
		return false;

	snd_pcm_hw_params_alloca (_hw);
	snd_pcm_sw_params_alloca (_sw);

	if (snd_device->string[0])
		pcmname = snd_device->string;
	if (snd_bits->int_val) {
		bps = snd_bits->int_val;
		if (bps != 16 && bps != 8) {
			Sys_Printf ("Error: invalid sample bits: %d\n", bps);
			return 0;
		}
	}
	if (snd_rate->int_val)
		rate = snd_rate->int_val;
	stereo = snd_stereo->int_val;
	if (!pcmname)
		pcmname = "default";
retry_open:
	err = qfsnd_pcm_open (&pcm, pcmname, SND_PCM_STREAM_PLAYBACK,
						  SND_PCM_NONBLOCK);
	if (0 > err) {
		Sys_Printf ("Error: audio open error: %s\n", qfsnd_strerror (err));
		return 0;
	}

	err = qfsnd_pcm_hw_params_any (pcm, hw);
	if (0 > err) {
		Sys_Printf ("ALSA: error setting hw_params_any. %s\n",
					qfsnd_strerror (err));
		goto error;
	}

	err = qfsnd_pcm_hw_params_set_access (pcm, hw,
										  SND_PCM_ACCESS_MMAP_INTERLEAVED);
	if (0 > err) {
		Sys_MaskPrintf (SYS_SND, "ALSA: Failure to set interleaved PCM "
						"access. %s\n", qfsnd_strerror (err));
		err = qfsnd_pcm_hw_params_set_access (pcm, hw,
										  SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
		if (0 > err) {
			Sys_MaskPrintf (SYS_SND, "ALSA: Failure to set noninterleaved PCM "
							"access. %s\n", qfsnd_strerror (err));
			// "default" did not work, so retry with "plughw". However do not
			// second guess the user, even if the user specified "default".
			if (!snd_device->string[0] && !strcmp (pcmname, "default")) {
				pcmname = "plughw";
				goto retry_open;
			}
			Sys_Printf ("ALSA: could not set mmap access\n");
			goto error;
		}
		sn.xfer = SNDDMA_ni_xfer;
	}
	Sys_Printf ("Using PCM %s.\n", pcmname);


	switch (bps) {
		case -1:
			err = qfsnd_pcm_hw_params_set_format (pcm, hw,
												  SND_PCM_FORMAT_S16_LE);
			if (0 <= err) {
				bps = 16;
			} else if (0 <= (err = qfsnd_pcm_hw_params_set_format (pcm, hw,
														 SND_PCM_FORMAT_U8))) {
				bps = 8;
			} else {
				Sys_Printf ("ALSA: no useable formats. %s\n",
							qfsnd_strerror (err));
				goto error;
			}
			break;
		case 8:
		case 16:
			err = qfsnd_pcm_hw_params_set_format (pcm, hw, bps == 8 ?
												  SND_PCM_FORMAT_U8 :
												  SND_PCM_FORMAT_S16);
			if (0 > err) {
				Sys_Printf ("ALSA: no usable formats. %s\n",
							qfsnd_strerror (err));
				goto error;
			}
			break;
		default:
			Sys_Printf ("ALSA: desired format not supported\n");
			goto error;
	}

	switch (stereo) {
		case -1:
			err = qfsnd_pcm_hw_params_set_channels (pcm, hw, 2);
			if (0 <= err) {
				stereo = 1;
			} else if (0 <= (err = qfsnd_pcm_hw_params_set_channels (pcm, hw,
																	 1))) {
				stereo = 0;
			} else {
				Sys_Printf ("ALSA: no usable channels. %s\n",
							qfsnd_strerror (err));
				goto error;
			}
			break;
		case 0:
		case 1:
			err = qfsnd_pcm_hw_params_set_channels (pcm, hw, stereo ? 2 : 1);
			if (0 > err) {
				Sys_Printf ("ALSA: no usable channels. %s\n",
							qfsnd_strerror (err));
				goto error;
			}
			break;
		default:
			Sys_Printf ("ALSA: desired channels not supported\n");
			goto error;
	}

	switch (rate) {
		case 0:
			{
				int         rates[] = {
								48000,
								44100,
								22050,
								11025,
								0
							};
				int         i;

				for (i = 0; rates[i]; i++) {
					rate = rates[i];
					Sys_MaskPrintf (SYS_SND, "ALSA: trying %dHz\n", rate);
					err = qfsnd_pcm_hw_params_set_rate_near (pcm, hw,
															 &rate, 0);
					if (0 <= err) {
						frag_size = 8 * bps * (rate / 11025);
						break;
					}
				}
				if (!rates[i]) {
					Sys_Printf ("ALSA: no usable rates. %s\n",
								qfsnd_strerror (err));
					goto error;
				}
			} break;
		case 11025:
		case 22050:
		case 44100:
		case 48000:
		default:
			err = qfsnd_pcm_hw_params_set_rate_near (pcm, hw, &rate, 0);
			if (0 > err) {
				Sys_Printf ("ALSA: desired rate %i not supported. %s\n", rate,
							qfsnd_strerror (err));
				goto error;
			}
			frag_size = 8 * bps * (rate / 11025);
			break;
	}

	err = qfsnd_pcm_hw_params_set_period_size_near (pcm, hw, &frag_size, 0);
	if (0 > err) {
		Sys_Printf ("ALSA: unable to set period size near %i. %s\n",
					(int) frag_size, qfsnd_strerror (err));
		goto error;
	}
	err = qfsnd_pcm_hw_params (pcm, hw);
	if (0 > err) {
		Sys_Printf ("ALSA: unable to install hw params: %s\n",
					qfsnd_strerror (err));
		goto error;
	}
	err = qfsnd_pcm_sw_params_current (pcm, sw);
	if (0 > err) {
		Sys_Printf ("ALSA: unable to determine current sw params. %s\n",
					qfsnd_strerror (err));
		goto error;
	}
	err = qfsnd_pcm_sw_params_set_start_threshold (pcm, sw, ~0U);
	if (0 > err) {
		Sys_Printf ("ALSA: unable to set playback threshold. %s\n",
					qfsnd_strerror (err));
		goto error;
	}
	err = qfsnd_pcm_sw_params_set_stop_threshold (pcm, sw, ~0U);
	if (0 > err) {
		Sys_Printf ("ALSA: unable to set playback stop threshold. %s\n",
					qfsnd_strerror (err));
		goto error;
	}
	err = qfsnd_pcm_sw_params (pcm, sw);
	if (0 > err) {
		Sys_Printf ("ALSA: unable to install sw params. %s\n",
					qfsnd_strerror (err));
		goto error;
	}

	memset ((dma_t *) &sn, 0, sizeof (sn));
	sn.channels = stereo + 1;

	// don't mix less than this in frames:
	err = qfsnd_pcm_hw_params_get_period_size (hw, (snd_pcm_uframes_t *)
											   (char *)
											   &sn.submission_chunk, 0);
	if (0 > err) {
		Sys_Printf ("ALSA: unable to get period size. %s\n",
					qfsnd_strerror (err));
		goto error;
	}

	sn.framepos = 0;
	sn.samplebits = bps;

	err = qfsnd_pcm_hw_params_get_buffer_size (hw, &buffer_size);
	if (0 > err) {
		Sys_Printf ("ALSA: unable to get buffer size. %s\n",
					qfsnd_strerror (err));
		goto error;
	}
	if (buffer_size != round_buffer_size (buffer_size)) {
		Sys_Printf ("ALSA: WARNING: non-power of 2 buffer size. sound may be unsatisfactory\n");
		Sys_Printf ("recommend using either the plughw, or hw devices or adjusting dmix\n");
		Sys_Printf ("to have a power of 2 buffer size\n");
	}

	sn.frames = buffer_size;
	sn.speed = rate;
	SNDDMA_GetDMAPos ();		//XXX sets sn.buffer
	Sys_Printf ("%5d channels\n", sn.channels);
	Sys_Printf ("%5d samples\n", sn.frames);
	Sys_Printf ("%5d samplepos\n", sn.framepos);
	Sys_Printf ("%5d samplebits\n", sn.samplebits);
	Sys_Printf ("%5d submission_chunk\n", sn.submission_chunk);
	Sys_Printf ("%5d speed\n", sn.speed);
	Sys_Printf ("0x%lx dma buffer\n", (long) sn.buffer);

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
	snd_pcm_uframes_t nframes = sn.frames;

	qfsnd_pcm_avail_update (pcm);
	qfsnd_pcm_mmap_begin (pcm, &areas, &offset, &nframes);
	sn.framepos = offset;
	sn.buffer = areas->addr;
	sn.xfer_data = (void *) areas;
	return sn.framepos;
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

	nframes = count;

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

PLUGIN_INFO(snd_output, alsa)
{
	plugin_info.type = qfp_snd_output;
	plugin_info.api_version = QFPLUGIN_VERSION;
	plugin_info.plugin_version = "0.1";
	plugin_info.description = "ALSA digital output";
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
