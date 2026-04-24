/*
	snd_ma.c

	miniaudio output driver

	Copyright (C) 2026 Bill Currie <bill@taniwha.org>

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

#define CINTERFACE

#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "snd_internal.h"

#define MINIAUDIO_IMPLEMENTATION
#define MA_USE_QUAD_MICROSOFT_CHANNEL_MAP
#include MINIAUDIO_H

typedef struct snd_ma_s {
	ma_device_config config;
	ma_device   device;
	void       *output;
	int         send_count;
} snd_ma_t;

static int sound_started = 0;
static int snd_blocked = 0;
static int snd_shutdown = 0;

static void
s_update(snd_t *snd)
{
}

static void
s_block_sound (snd_t *snd)
{
	if (!sound_started)
		return;
	if (++snd_blocked == 1) {
	}
}

static void
s_unblock_sound (snd_t *snd)
{
	if (!sound_started)
		return;
	if (!snd_blocked)
		return;

	if (!--snd_blocked) {
	}
}

static void
snd_ma_xfer (snd_t *snd, portable_samplepair_t *paintbuffer, int count,
			 float volume)
{
	snd_ma_t *ma = snd->xfer_data;
	float *output = ma->output;

	int base = ma->send_count;
	ma->send_count += count;
	if (snd_blocked) {
		for (int i = 0; i < count; i++) {
			output[(base + i) * 2 + 0] = 0;
			output[(base + i) * 2 + 1] = 0;
		}
		return;
	}
	for (int i = 0; i < count; i++) {
		output[(base + i) * 2 + 0] = volume * paintbuffer[i].left;
		output[(base + i) * 2 + 1] = volume * paintbuffer[i].right;
	}
}

/*
 * snd_ma_process is the callback to minidaudio
*/
static void
snd_ma_process (ma_device *device, void *output, const void *input, ma_uint32 frame_count)
{
	snd_t *snd = device->pUserData;
	snd_ma_t *ma = snd->xfer_data;
	ma->output = output;
	ma->send_count = 0;
	snd->paint_channels(snd, snd->paintedtime + frame_count);
}

static void
s_shutdown (snd_t *snd)
{
	snd_shutdown = 1;
	snd->finish_channels ();

	snd_ma_t *ma = snd->xfer_data;
	ma_device_stop(&ma->device);
	ma_device_uninit(&ma->device);
	free(ma);
	snd->xfer_data = nullptr;
}

/*
	SNDDMA_Init

	Try to find a sound device to mix for.
	Returns false if nothing is found.
*/
static int
s_init (snd_t *snd)
{
	snd_ma_t *ma = malloc(sizeof(snd_ma_t));
	ma->config = ma_device_config_init(ma_device_type_playback);
	ma->config.playback.format = ma_format_f32;
	ma->config.playback.channels = 2;
	ma->config.sampleRate = 48000;
	ma->config.dataCallback = snd_ma_process;
	ma->config.pUserData = snd;

	if (ma_device_init(NULL, &ma->config, &ma->device) != MA_SUCCESS) {
		Sys_MaskPrintf(SYS_snd_ma, "Miniaudio device failed to init");
		free(ma);
		return 0;
	}

	snd->speed = ma->device.sampleRate;
	snd->channels = ma->device.playback.channels;
	snd->xfer_data = ma;
	snd->xfer      = snd_ma_xfer;
	ma_device_start(&ma->device);
	sound_started = 1;

	Sys_MaskPrintf(SYS_snd_ma, "Miniaudio initialized\n");
	return 1;
}

static snd_output_data_t plugin_info_snd_output_data = {
	.model = som_pull,
};

static snd_output_funcs_t plugin_info_snd_output_funcs = {
	.init          = s_init,
	.shutdown      = s_shutdown,
	.on_update     = s_update,
	.block_sound   = s_block_sound,
	.unblock_sound = s_unblock_sound,
};

static general_data_t plugin_info_general_data = {
};

static general_funcs_t plugin_info_general_funcs = {
	//.init = s_init_cvars,
};

static plugin_data_t plugin_info_data = {
	.general    = &plugin_info_general_data,
	.snd_output = &plugin_info_snd_output_data,
};

static plugin_funcs_t plugin_info_funcs = {
	.general    = &plugin_info_general_funcs,
	.snd_output = &plugin_info_snd_output_funcs,
};

static plugin_t plugin_info = {
	qfp_snd_output,
	0,
	QFPLUGIN_VERSION,
	"0.1",
	"Miniaudio Sound Renderer",
	"Copyright (C) 2026 contributors of the QuakeForge "
	"project\n"
	"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(snd_output, ma)
{
	return &plugin_info;
}
