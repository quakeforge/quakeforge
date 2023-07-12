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

typedef struct {
	const snd_pcm_channel_area_t *areas;
	snd_pcm_uframes_t offset;
	snd_pcm_uframes_t nframes;
} alsa_pkt_t;

static int			snd_inited;
static int			snd_blocked = 0;
static snd_pcm_uframes_t buffer_size;

static void		   *alsa_handle;
static snd_pcm_t   *pcm;
static snd_async_handler_t *async_handler;

static int snd_bits;
static cvar_t snd_bits_cvar = {
	.name = "snd_bits",
	.description =
		"sound sample depth. 0 is system default",
	.default_value = "0",
	.flags = CVAR_ROM,
	.value = { .type = &cexpr_int, .value = &snd_bits },
};
static char *snd_device;
static cvar_t snd_device_cvar = {
	.name = "snd_device",
	.description =
		"sound device. \"\" is system default",
	.default_value = "",
	.flags = CVAR_ROM,
	.value = { .type = 0, .value = &snd_device },
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

//FIXME xfer probably should not be touching this (such data should probably
//come through snd_t)
static snd_output_data_t plugin_info_snd_output_data;

#define QF_ALSA_NEED(ret, func, params) \
static ret (*qf##func) params;
#include "alsa_funcs_list.h"
#undef QF_ALSA_NEED

static bool
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
	Cvar_Register (&snd_stereo_cvar, 0, 0);
	Cvar_Register (&snd_rate_cvar, 0, 0);
	Cvar_Register (&snd_device_cvar, 0, 0);
	Cvar_Register (&snd_bits_cvar, 0, 0);
}

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
alsa_ni_xfer (snd_t *snd, portable_samplepair_t *paintbuffer, int count,
			  float volume)
{
	const snd_pcm_channel_area_t *areas;
	int         out_idx, out_max;
	float      *p;

	areas = snd->xfer_data;

	p = (float *) paintbuffer;
	out_max = snd->frames - 1;
	out_idx = *plugin_info_snd_output_data.paintedtime;
	while (out_idx > out_max)
		out_idx -= out_max + 1;

	if (snd->samplebits == 16) {
		short      *out_0 = (short *) areas[0].addr;
		short      *out_1 = (short *) areas[1].addr;

		if (snd->channels == 2) {
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
	} else if (snd->samplebits == 8) {
		byte       *out_0 = (byte *) areas[0].addr;
		byte       *out_1 = (byte *) areas[1].addr;

		if (snd->channels == 2) {
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

static void
alsa_xfer (snd_t *snd, portable_samplepair_t *paintbuffer, int count,
		   float volume)
{
	int			out_idx, out_max, step, val;
	float	   *p;
	alsa_pkt_t *packet = snd->xfer_data;;

	p = (float *) paintbuffer;
	count *= snd->channels;
	out_max = (snd->frames * snd->channels) - 1;
	out_idx = snd->paintedtime * snd->channels;
	while (out_idx > out_max)
		out_idx -= out_max + 1;
	step = 3 - snd->channels;

	if (snd->samplebits == 16) {
		short      *out = (short *) packet->areas[0].addr;

		while (count--) {
			val = (*p * volume) * 0x8000;
			p += step;
			if (val > 0x7fff)
				val = 0x7fff;
			else if (val < -0x8000)
				val = -0x8000;
			out[out_idx++] = val;
			if (out_idx > out_max)
				out_idx = 0;
		}
	} else if (snd->samplebits == 8) {
		unsigned char *out = (unsigned char *) packet->areas[0].addr;

		while (count--) {
			val = (*p * volume) * 128;
			p += step;
			if (val > 0x7f)
				val = 0x7f;
			else if (val < -0x80)
				val = -0x80;
			out[out_idx++] = val + 0x80;
			if (out_idx > out_max)
				out_idx = 0;
		}
	}
}

static int
alsa_recover (snd_pcm_t *pcm, int err)
{
	if (err == -EPIPE) {
		Sys_Printf ("snd_alsa: xrun\n");
		// xrun
		if ((err = qfsnd_pcm_prepare (pcm)) < 0) {
			Sys_MaskPrintf (SYS_snd, "snd_alsa: recover from xrun failed: %s\n",
							qfsnd_strerror (err));
			return err;
		}
		return 0;
	} else if (err == -ESTRPIPE) {
		Sys_Printf ("snd_alsa: suspend\n");
		// suspend
		while ((err = qfsnd_pcm_resume(pcm)) == -EAGAIN) {
			usleep (20 * 1000);
		}
		if (err < 0 && (err = qfsnd_pcm_prepare (pcm)) < 0) {
			Sys_MaskPrintf (SYS_snd,
							"snd_alsa: recover from suspend failed: %s\n",
							qfsnd_strerror (err));
			return err;
		}
		return 0;
	}
	return err;
}

static int
alsa_process (snd_pcm_t *pcm, snd_t *snd)
{
	alsa_pkt_t  packet;
	int         res;
	int         ret = 1;
	snd_pcm_uframes_t size = snd->submission_chunk;

	snd->xfer_data = &packet;
	while (size > 0) {
		packet.nframes = size;
		if ((res = qfsnd_pcm_mmap_begin (pcm, &packet.areas, &packet.offset,
										 &packet.nframes)) < 0) {
			if ((res = alsa_recover (pcm, -EPIPE)) < 0) {
				Sys_Printf ("snd_alsa: XRUN recovery failed: %s\n",
							qfsnd_strerror (res));
				snd->xfer_data = 0;
				return res;
			}
			ret = 0;
		}
		snd->buffer = packet.areas[0].addr;
		snd->paint_channels (snd, snd->paintedtime + packet.nframes);
		if ((res = qfsnd_pcm_mmap_commit (pcm, packet.offset,
										  packet.nframes)) < 0
			|| (snd_pcm_uframes_t) res != packet.nframes) {
			if ((res = alsa_recover (pcm, res >= 0 ? -EPIPE : res)) < 0) {
				Sys_Printf ("snd_alsa: XRUN recovery failed: %s\n",
							qfsnd_strerror (res));
				snd->xfer_data = 0;
				return res;
			}
			ret = 0;
		}
		size -= packet.nframes;
	}

	snd->xfer_data = 0;
	return ret;
}

static void
alsa_callback (snd_async_handler_t *handler)
{
	snd_pcm_t  *pcm = qfsnd_async_handler_get_pcm (handler);
	snd_t      *snd = qfsnd_async_handler_get_callback_private (handler);
	int         res;
	int         avail;
	int         first = 0;

	while (1) {
		snd_pcm_state_t state = qfsnd_pcm_state (pcm);
		if (state == SND_PCM_STATE_XRUN) {
			if ((res = alsa_recover (pcm, -EPIPE)) < 0) {
				Sys_Printf ("snd_alsa: XRUN recovery failed: %s\n",
							qfsnd_strerror (res));
				//FIXME disable/restart sound
				return;
			}
		} else if (state == SND_PCM_STATE_SUSPENDED) {
			if ((res = alsa_recover (pcm, -EPIPE)) < 0) {
				Sys_Printf ("snd_alsa: suspend recovery failed: %s\n",
							qfsnd_strerror (res));
				//FIXME disable/restart sound
				return;
			}
		}
		if ((avail = qfsnd_pcm_avail_update (pcm)) < 0) {
			if ((res = alsa_recover (pcm, -EPIPE)) < 0) {
				Sys_Printf ("snd_alsa: avail update failed: %s\n",
							qfsnd_strerror (res));
				//FIXME disable/restart sound
				return;
			}
			first = 1;
			continue;
		}
		if (avail < snd->submission_chunk) {
			if (first) {
				first = 0;
				if ((res = qfsnd_pcm_start (pcm)) < 0) {
					Sys_Printf ("snd_alsa: start failed: %s\n",
								qfsnd_strerror (res));
					return;
				}
				continue;
			}
			break;
		}

		if ((res = alsa_process (pcm, snd))) {
			if (res < 0) {
				//FIXME disable/restart sound
				return;
			}
			break;
		}
		first = 1;
	}
}

static int
alsa_open_playback (snd_t *snd, const char *device)
{
	if (!*device) {
		device = "default";
	}
	Sys_Printf ("Using PCM %s.\n", device);

	int         res = qfsnd_pcm_open (&pcm, device, SND_PCM_STREAM_PLAYBACK,
									  SND_PCM_NONBLOCK);
	if (res < 0) {
		Sys_Printf ("snd_alsa: audio open error: %s\n", qfsnd_strerror (res));
		return 0;
	}

	return 1;
}

static int
alsa_playback_set_mmap (snd_t *snd, snd_pcm_hw_params_t	*hw)
{
	int         res;

	res = qfsnd_pcm_hw_params_set_access (pcm, hw,
										  SND_PCM_ACCESS_MMAP_INTERLEAVED);
	if (res == 0) {
		snd->xfer = alsa_xfer;
		return 1;
	}
	Sys_MaskPrintf (SYS_snd, "snd_alsa: Failure to set interleaved PCM "
					"access. (%d) %s\n", res, qfsnd_strerror (res));
	res = qfsnd_pcm_hw_params_set_access (pcm, hw,
										  SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
	if (res == 0) {
		snd->xfer = alsa_ni_xfer;
		return 1;
	}
	Sys_MaskPrintf (SYS_snd, "snd_alsa: Failure to set noninterleaved PCM "
					"access. (%d) %s\n", res, qfsnd_strerror (res));
	Sys_Printf ("snd_alsa: could not set mmap access\n");

	return 0;
}

static int
alsa_playback_set_bps (snd_t *snd, snd_pcm_hw_params_t *hw)
{
	int         res;
	int         bps = 0;

	if (snd_bits == 16) {
		bps = SND_PCM_FORMAT_S16_LE;
		snd->samplebits = 16;
	} else if (snd_bits == 8) {
		bps = SND_PCM_FORMAT_U8;
		snd->samplebits = 8;
	} else if (snd_bits) {
		Sys_Printf ("snd_alsa: invalid sample bits: %d\n", bps);
		return 0;
	}

	if (bps) {
		if ((res = qfsnd_pcm_hw_params_set_format (pcm, hw, bps)) == 0) {
			return 1;
		}
	} else {
		bps = SND_PCM_FORMAT_S16_LE;
		if ((res = qfsnd_pcm_hw_params_set_format (pcm, hw, bps)) == 0) {
			snd->samplebits = 16;
			return 1;
		}
		bps = SND_PCM_FORMAT_U8;
		if ((res = qfsnd_pcm_hw_params_set_format (pcm, hw, bps)) == 0) {
			snd->samplebits = 8;
			return 1;
		}
		Sys_Printf ("snd_alsa: no usable formats. %s\n", qfsnd_strerror (res));
	}
	snd->samplebits = -1;
	Sys_Printf ("snd_alsa: desired format not supported\n");
	return 0;
}

static int
alsa_playback_set_channels (snd_t *snd, snd_pcm_hw_params_t *hw)
{
	int         res;
	int         channels = 1;

	if (snd_stereo) {
		channels = 2;
	}
	if ((res = qfsnd_pcm_hw_params_set_channels (pcm, hw, channels)) == 0) {
		snd->channels = channels;
		return 1;
	}

	Sys_Printf ("snd_alsa: desired channels not supported\n");
	return 0;
}

static int
alsa_playback_set_rate (snd_t *snd, snd_pcm_hw_params_t *hw)
{
	int         res;
	unsigned    rate = 0;
	static int  default_rates[] = { 48000, 44100, 22050, 11025, 0 };

	if (snd_rate) {
		rate = snd_rate;
	}

	if (rate) {
		if ((res = qfsnd_pcm_hw_params_set_rate (pcm, hw, rate, 0)) == 0) {
			snd->speed = rate;
			return 1;
		}
		Sys_Printf ("snd_alsa: desired rate %i not supported. %s\n", rate,
					qfsnd_strerror (res));
	} else {
		// use default rate
		int         dir = 0;
		for (int *def_rate = default_rates; *def_rate; def_rate++) {
			rate = *def_rate;
			res = qfsnd_pcm_hw_params_set_rate_near (pcm, hw, &rate, &dir);
			if (res == 0) {
				snd->speed = rate;
				return 1;
			}
		}
		Sys_Printf ("snd_alsa: no usable rate\n");
	}

	return 0;
}

static int
alsa_playback_set_period_size (snd_t *snd, snd_pcm_hw_params_t *hw)
{
	int         res;
	snd_pcm_uframes_t period_size;

	// works out to about 5.5ms (5.3 for 48k, 5.8 for 44k1) but consistent for
	// different sample rates give or take rounding
	period_size = 64 * (snd->speed / 11025);
	res = qfsnd_pcm_hw_params_set_period_size_near (pcm, hw, &period_size, 0);
	if (res == 0) {
		// don't mix less than this in frames:
		res = qfsnd_pcm_hw_params_get_period_size (hw, &period_size, 0);
		if (res == 0) {
			snd->submission_chunk = period_size;
			return 1;
		}
		Sys_Printf ("snd_alsa: unable to get period size. %s\n",
					qfsnd_strerror (res));
	} else {
		Sys_Printf ("snd_alsa: unable to set period size near %i. %s\n",
					(int) period_size, qfsnd_strerror (res));
	}
	return 0;
}

static int
SNDDMA_Init (snd_t *snd)
{
	int         res;
	const char *device = snd_device;

	snd_pcm_hw_params_t *hw;
	snd_pcm_sw_params_t *sw;

	if (!load_libasound ())
		return false;

	snd_pcm_hw_params_alloca (&hw);
	snd_pcm_sw_params_alloca (&sw);


	while (1) {
		if (!alsa_open_playback (snd, device)) {
			return 0;
		}
		if ((res = qfsnd_pcm_hw_params_any (pcm, hw)) < 0) {
			Sys_Printf ("snd_alsa: error setting hw_params_any. %s\n",
						qfsnd_strerror (res));
			goto error;
		}
		if (alsa_playback_set_mmap (snd, hw)) {
			break;
		}
		if (*device) {
			goto error;
		}
		qfsnd_pcm_close (pcm);
		device = "plughw";
	}

	if (!alsa_playback_set_bps (snd, hw)) {
		goto error;
	}

	if (!alsa_playback_set_channels (snd, hw)) {
		goto error;
	}

	if (!alsa_playback_set_rate (snd, hw)) {
		goto error;
	}

	if (!alsa_playback_set_period_size (snd, hw)) {
		goto error;
	}

	if ((res = qfsnd_pcm_hw_params (pcm, hw)) < 0) {
		Sys_Printf ("snd_alsa: unable to install hw params: %s\n",
					qfsnd_strerror (res));
		goto error;
	}

	if ((res = qfsnd_pcm_sw_params_current (pcm, sw)) < 0) {
		Sys_Printf ("snd_alsa: unable to determine current sw params. %s\n",
					qfsnd_strerror (res));
		goto error;
	}
	if ((res = qfsnd_pcm_sw_params_set_start_threshold (pcm, sw, ~0U)) < 0) {
		Sys_Printf ("snd_alsa: unable to set playback threshold. %s\n",
					qfsnd_strerror (res));
		goto error;
	}
	if ((res = qfsnd_pcm_sw_params_set_stop_threshold (pcm, sw, ~0U)) < 0) {
		Sys_Printf ("snd_alsa: unable to set playback stop threshold. %s\n",
					qfsnd_strerror (res));
		goto error;
	}
	if ((res = qfsnd_pcm_sw_params (pcm, sw)) < 0) {
		Sys_Printf ("snd_alsa: unable to install sw params. %s\n",
					qfsnd_strerror (res));
		goto error;
	}

	snd->framepos = 0;

	if ((res = qfsnd_pcm_hw_params_get_buffer_size (hw, &buffer_size)) < 0) {
		Sys_Printf ("snd_alsa: unable to get buffer size. %s\n",
					qfsnd_strerror (res));
		goto error;
	}
	if (buffer_size != round_buffer_size (buffer_size)) {
		Sys_Printf ("snd_alsa: WARNING: non-power of 2 buffer size. sound may be unsatisfactory\n");
		Sys_Printf ("recommend using either the plughw, or hw devices or adjusting dmix\n");
		Sys_Printf ("to have a power of 2 buffer size\n");
	}

	if ((res = qfsnd_async_add_pcm_handler (&async_handler, pcm,
										    alsa_callback, snd)) < 0) {
		Sys_Printf ("snd_alsa: unable to register async handler: %s",
					qfsnd_strerror (res));
		goto error;
	}

	snd->frames = buffer_size;

	snd->threaded = 1;//XXX FIXME double check whether it's always true

	// send the first period to fill the buffer
	// also sets snd->buffer
	if (alsa_process (pcm, snd) < 0) {
		goto error;
	}

	qfsnd_pcm_start (pcm);

	Sys_Printf ("%5d channels %sinterleaved\n", snd->channels,
				snd->xfer ? "non-" : "");
	Sys_Printf ("%5d samples (%.1fms)\n", snd->frames,
				1000.0 * snd->frames / snd->speed);
	Sys_Printf ("%5d samplepos\n", snd->framepos);
	Sys_Printf ("%5d samplebits\n", snd->samplebits);
	Sys_Printf ("%5d submission_chunk (%.1fms)\n", snd->submission_chunk,
				1000.0 * snd->submission_chunk / snd->speed);
	Sys_Printf ("%5d speed\n", snd->speed);
	Sys_Printf ("0x%lx dma buffer\n", (long) snd->buffer);

	snd_inited = 1;

	return 1;
error:
	qfsnd_pcm_close (pcm);
	snd->channels = 0;
	snd->frames = 0;
	snd->samplebits = 0;
	snd->submission_chunk = 0;
	snd->speed = 0;
	return 0;
}

static void
SNDDMA_shutdown (snd_t *snd)
{
	if (snd_inited) {
		qfsnd_async_del_handler (async_handler);
		async_handler = 0;
		qfsnd_pcm_close (pcm);
		snd_inited = 0;
	}
}

static void
SNDDMA_BlockSound (snd_t *snd)
{
	if (snd_inited && ++snd_blocked == 1)
		qfsnd_pcm_pause (pcm, 1);
}

static void
SNDDMA_UnblockSound (snd_t *snd)
{
	if (!snd_inited || !snd_blocked)
		return;
	if (!--snd_blocked)
		qfsnd_pcm_pause (pcm, 0);
}

static general_data_t plugin_info_general_data = {
};

static general_funcs_t plugin_info_general_funcs = {
	.init = SNDDMA_Init_Cvars,
	.shutdown = NULL,
};

static snd_output_data_t plugin_info_snd_output_data = {
	.model = som_pull,
};

static snd_output_funcs_t plugin_info_snd_output_funcs = {
	.init = SNDDMA_Init,
	.shutdown = SNDDMA_shutdown,
	.block_sound = SNDDMA_BlockSound,
	.unblock_sound = SNDDMA_UnblockSound,
};

static plugin_data_t plugin_info_data = {
	.general = &plugin_info_general_data,
	.snd_output = &plugin_info_snd_output_data,
};

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.snd_output = &plugin_info_snd_output_funcs,
};

static plugin_t plugin_info = {
	.type = qfp_snd_output,
	.api_version = QFPLUGIN_VERSION,
	.plugin_version = "0.1",
	.description = "ALSA digital output",
	.copyright = "Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001  contributors of the QuakeForge "
		"project\n"
		"Please see the file \"AUTHORS\" for a list of contributors",
	.functions = &plugin_info_funcs,
	.data = &plugin_info_data,
};

PLUGIN_INFO(snd_output, alsa)
{
	return &plugin_info;
}
