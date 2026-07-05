/*
	snd_default.c

	main control for any streaming sound output device

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
		Boston, MA	02111-1307, USA

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
#include <inttypes.h>
#include <stdlib.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/dstring.h"
#include "QF/model.h"
#include "QF/qargs.h"
#include "QF/sys.h"
#include "QF/sound.h"
#include "QF/plugin.h"
#include "QF/va.h"
#include "QF/quakefs.h"

#include "QF/scene/transform.h"

#include "snd_internal.h"

static int      snd_blocked = 0;			// counter!

static unsigned soundtime;					// sample PAIRS

static bool     sound_started = false;


float snd_volume;
static cvar_t snd_volume_cvar = {
	.name = "volume",
	.description =
		"Set the volume for sound playback.",
	.default_value = "0.7",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &snd_volume },
};
static int nosound;
static cvar_t nosound_cvar = {
	.name = "nosound",
	.description =
		"Set to turn sound off.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &nosound },
};
static float snd_mixahead;
static cvar_t snd_mixahead_cvar = {
	.name = "snd_mixahead",
	.description =
		"Delay time for sounds.",
	.default_value = "0.1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_float, .value = &snd_mixahead },
};
static int snd_noextraupdate;
static cvar_t snd_noextraupdate_cvar = {
	.name = "snd_noextraupdate",
	.description =
		"Toggles the correct value display in host_speeds. Usually messes up "
		"sound playback when in effect.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &snd_noextraupdate },
};
static int snd_show;
static cvar_t snd_show_cvar = {
	.name = "snd_show",
	.description =
		"Toggles display of sounds currently being played.",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_int, .value = &snd_show },
};

static general_data_t plugin_info_general_data;

static snd_output_funcs_t *snd_output_funcs;
static snd_output_data_t *snd_output_data;

static snd_t    snd = {
	.finish_channels = SND_FinishChannels,
	.paint_channels  = SND_PaintChannels,
};
static bool     snd_shutdown = false;

static void
s_xfer_paint_buffer (snd_t *snd, portable_samplepair_t *paintbuffer, int count,
					 float volume)
{
	qfZoneScoped (true);
	int			out_idx, out_max, step, val;
	float	   *p;

	p = (float *) paintbuffer;
	count *= snd->channels;
	out_max = (snd->frames * snd->channels) - 1;
	out_idx = snd->paintedtime * snd->channels;
	while (out_idx > out_max)
		out_idx -= out_max + 1;
	step = 3 - snd->channels;

	if (snd->samplebits == 16) {
		short      *out = (short *) snd->buffer;

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
		unsigned char *out = (unsigned char *) snd->buffer;

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

static void
s_clear_buffer (snd_t *snd)
{
	qfZoneScoped (true);
	if (!sound_started || !snd || !snd->buffer)
		return;

	int zero = 0;
	if (snd->samplebits == 8) {
		zero = 0x80;
	}

	int count = snd->frames * snd->channels * snd->samplebits / 8;
	for (int i = 0; i < count; i++)
		snd->buffer[i] = zero;
}

static void
s_stop_all_sounds (void)
{
	qfZoneScoped (true);
	SND_StopAllSounds (&snd);
	SND_ScanChannels (&snd, snd.threaded);
	s_clear_buffer (&snd);
}

//=============================================================================

static void
s_get_soundtime (void)
{
	qfZoneScoped (true);
	int			frames, framepos;
	static int	buffers, oldframepos;

	frames = snd.frames;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to s_update.  Oh well.
	if ((framepos = snd_output_funcs->get_dma_pos (&snd)) == -1)
		return;

	if (framepos < oldframepos) {
		buffers++;						// buffer wrapped

		if (snd.paintedtime > 0x40000000) {	// time to chop things off to avoid
			// 32 bit limits
			buffers = 0;
			snd.paintedtime = frames;
			s_stop_all_sounds ();
		}
	}
	oldframepos = framepos;

	soundtime = buffers * frames + framepos;
}

static void
s_update_ (void)
{
	qfZoneScoped (true);
	unsigned int	endtime, samps;

	if (!sound_started || (snd_blocked > 0))
		return;

	// Updates DMA time
	s_get_soundtime ();

	// check to make sure that we haven't overshot
	if (snd.paintedtime < soundtime) {
//		Sys_Printf ("S_Update_ : overflow\n");
		snd.paintedtime = soundtime;
	}
	// mix ahead of current position
	endtime = soundtime + snd_mixahead * snd.speed;
	samps = snd.frames;
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	SND_PaintChannels (&snd, endtime);
	snd_output_funcs->submit (&snd);
}

/*
	s_update

	Called once each time through the main loop
*/
static void
s_update (transform_t ear, const byte *ambient_sound_level)
{
	qfZoneScoped (true);
	if (!sound_started || (snd_blocked > 0))
		return;

	if (snd_output_funcs->on_update) {
		snd_output_funcs->on_update (&snd);
	}

	SND_SetListener (&snd, ear, ambient_sound_level);

	if (snd_output_data->model == som_push) {
		// mix some sound
		s_update_ ();
		SND_ScanChannels (&snd, false);
	}
}

static void
s_extra_update (void)
{
	qfZoneScoped (true);
	if (snd_output_data->model == som_push) {
		if (!sound_started || snd_noextraupdate)
			return;							// don't pollute timings
		s_update_ ();
	}
}

static void
s_block_sound (void)
{
	qfZoneScoped (true);
	if (++snd_blocked == 1) {
		snd_output_funcs->block_sound (&snd);
		s_clear_buffer (&snd);
	}
}

static void
s_unblock_sound (void)
{
	qfZoneScoped (true);
	if (!snd_blocked)
		return;

	if (!--snd_blocked) {
		s_clear_buffer (&snd);
		snd_output_funcs->unblock_sound (&snd);
	}
}

/* console functions */

static void
s_soundinfo_f (void)
{
	qfZoneScoped (true);
	if (!sound_started) {
		Sys_Printf ("sound system not started\n");
		return;
	}

	Sys_Printf ("%5d channels\n", snd.channels);
	Sys_Printf ("%5d frames (%.1fms)\n", snd.frames,
				1000.0 * snd.frames / snd.speed);
	Sys_Printf ("%5d framepos\n", snd.framepos);
	Sys_Printf ("%5d samplebits\n", snd.samplebits);
	Sys_Printf ("%5d submission_chunk (%.1fms)\n", snd.submission_chunk,
				1000.0 * snd.submission_chunk / snd.speed);
	Sys_Printf ("%5d speed\n", snd.speed);
	Sys_Printf ("0x%"PRIxPTR" dma buffer\n",  (intptr_t) snd.buffer);
	Sys_Printf ("%5d total_channels\n", snd_total_channels);
}

static void
s_stop_all_sounds_f (void)
{
	qfZoneScoped (true);
	s_stop_all_sounds ();
}

static void
s_startup (void)
{
	qfZoneScoped (true);
	if (!SND_Memory_Init ()) {
		return;
	}
	if (!SND_Fill_Init ()) {
		return;
	}
	if (!snd_output_funcs->init (&snd)) {
		Sys_Printf ("S_Startup: output init failed.\n");
		return;
	}
	if (!snd.xfer)
		snd.xfer = s_xfer_paint_buffer;

	sound_started = true;
}

static void
s_snd_force_unblock (void)
{
	qfZoneScoped (true);
	snd_blocked = 1;
	s_unblock_sound ();
}

static void
s_init_cvars (void)
{
	qfZoneScoped (true);
	Cvar_Register (&nosound_cvar, nullptr, nullptr);
	Cvar_Register (&snd_volume_cvar, nullptr, nullptr);
	Cvar_Register (&snd_mixahead_cvar, nullptr, nullptr);
	Cvar_Register (&snd_noextraupdate_cvar, nullptr, nullptr);
	Cvar_Register (&snd_show_cvar, nullptr, nullptr);

	SND_Memory_Init_Cvars ();
}

static void
s_init (void)
{
	qfZoneScoped (true);
	snd_output_funcs = snd_render_data.output->functions->snd_output;
	snd_output_data = snd_render_data.output->data->snd_output;
	snd_render_data.soundtime = &soundtime;
	Sys_Printf ("\nSound Initialization\n");

	Cmd_AddCommand ("stopsound", s_stop_all_sounds_f,
					"Stops all sounds currently being played");
	Cmd_AddCommand ("soundinfo", s_soundinfo_f,
					"Report information on the sound system");
	Cmd_AddCommand ("snd_force_unblock", s_snd_force_unblock,
					"fix permanently blocked sound");

	s_startup ();

	if (!sound_started) {				// sound startup failed? Bail out.
		return;
	}

	SND_SFX_Init (&snd);
	SND_Channels_Init (&snd);

	s_stop_all_sounds ();
}

static void
s_shutdown (void)
{
	qfZoneScoped (true);
	if (!sound_started)
		return;

	sound_started = false;
	snd_shutdown = true;

	SND_SFX_Shutdown (&snd);
	SND_Fill_Shutdown ();
	snd_output_funcs->shutdown (&snd);
}

static void
s_ambient_off (void)
{
	qfZoneScoped (true);
	if (!sound_started)
		return;
	SND_AmbientOff (&snd);
}

static void
s_ambient_on (void)
{
	qfZoneScoped (true);
	if (!sound_started)
		return;
	SND_AmbientOn (&snd);
}

static void
s_set_ambient (int amb_channel, sfx_t *sfx)
{
	qfZoneScoped (true);
	if (!sound_started)
		return;
	SND_SetAmbient (&snd, amb_channel, sfx);
}

static void
s_static_sound (sfx_t *sfx, vec4f_t origin, float vol,
				float attenuation)
{
	qfZoneScoped (true);
	if (!sound_started)
		return;
	SND_StaticSound (&snd, sfx, origin, vol, attenuation);
}

static void
s_start_sound (int entnum, int entchannel, sfx_t *sfx, vec4f_t origin,
			   float vol, float attenuation)
{
	qfZoneScoped (true);
	if (!sound_started)
		return;
	if (!snd_shutdown)
		SND_StartSound (&snd, entnum, entchannel, sfx, origin, vol,
						attenuation);
}

static void
s_stop_sound (int entnum, int entchannel)
{
	qfZoneScoped (true);
	if (!sound_started)
		return;
	SND_StopSound (&snd, entnum, entchannel);
}

static sfx_t *
s_precache_sound (const char *name)
{
	qfZoneScoped (true);
	if (!sound_started)
		return nullptr;
	return SND_PrecacheSound (&snd, name);
}

static sfx_t *
s_load_sound (const char *name)
{
	qfZoneScoped (true);
	if (!sound_started)
		return nullptr;
	return SND_LoadSound (&snd, name);
}

static void
s_channel_free (channel_t *chan)
{
	qfZoneScoped (true);
	if (!sound_started)
		return;
	SND_ChannelStop (&snd, chan);
}

static bool
s_channel_set_sfx (channel_t *chan, sfx_t *sfx)
{
	qfZoneScoped (true);
	sfxbuffer_t *buffer = sfx->open (sfx);
	if (!buffer) {
		return false;
	}
	chan->buffer = buffer;
	return true;
}

static void
s_channel_set_paused (channel_t *chan, bool paused)
{
	qfZoneScoped (true);
	chan->pause = paused;
}

static void
s_channel_set_looping (channel_t *chan, bool looping)
{
	qfZoneScoped (true);
	// FIXME implement
}

static chan_state
s_channel_get_state (channel_t *chan)
{
	qfZoneScoped (true);
	// stop means the channel has been "freed" and is waiting for the mixer
	// thread to be done with it, thus putting the channel in an invalid state
	// from the user's point of view. ie, don't touch (user should set channel
	// pointer to null).
	if (!chan->stop) {
		if (chan->done) {
			// The mixer has finished mixing the channel (come to the end).
			return chan_done;
		}
		if (!chan->buffer) {
			// channel has not been started yet
			return chan_pending;
		}
		if (chan->pause) {
			return chan_paused;
		}
		return chan_playing;
	}
	return chan_invalid;
}

static void
s_channel_set_volume (channel_t *chan, float volume)
{
	qfZoneScoped (true);
	SND_ChannelSetVolume (chan, volume);
}

static void
s_local_sound (const char *sound)
{
	qfZoneScoped (true);
	if (!sound_started)
		return;
	if (!snd_shutdown)
		SND_LocalSound (&snd, sound);
}

static channel_t *
s_alloc_channel (void)
{
	qfZoneScoped (true);
	if (!sound_started)
		return nullptr;
	if (!snd_shutdown)
		return SND_AllocChannel (&snd);
	return nullptr;
}

static general_funcs_t plugin_info_general_funcs = {
	.init = s_init_cvars,
	.shutdown = s_shutdown,
};

static snd_render_funcs_t plugin_info_render_funcs = {
	.init = s_init,
	.ambient_off     = s_ambient_off,
	.ambient_on      = s_ambient_on,
	.set_ambient     = s_set_ambient,
	.static_sound    = s_static_sound,
	.start_sound     = s_start_sound,
	.local_sound     = s_local_sound,
	.stop_sound      = s_stop_sound,

	.alloc_channel   = s_alloc_channel,
	.channel_free    = s_channel_free,
	.channel_set_sfx = s_channel_set_sfx,
	.channel_set_paused  = s_channel_set_paused,
	.channel_set_looping = s_channel_set_looping,
	.channel_get_state   = s_channel_get_state,
	.channel_set_volume  = s_channel_set_volume,

	.precache_sound  = s_precache_sound,
	.load_sound      = s_load_sound,

	.update          = s_update,
	.stop_all_sounds = s_stop_all_sounds,
	.extra_update    = s_extra_update,
	.block_sound     = s_block_sound,
	.unblock_sound   = s_unblock_sound,
};

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.snd_render = &plugin_info_render_funcs,
};

snd_render_data_t snd_render_data = {
	.paintedtime = &snd.paintedtime,
};

static plugin_data_t plugin_info_data = {
	.general = &plugin_info_general_data,
	.snd_render = &snd_render_data,
};

static plugin_t plugin_info = {
	.type = qfp_snd_render,
	.api_version = QFPLUGIN_VERSION,
	.plugin_version = "0.1",
	.description = "Sound Renderer",
	.copyright = "Copyright (C) 1996-1997 id Software, Inc.\n"
				 "Copyright (C) 1999,2000,2001  contributors of the "
				 "QuakeForge project\n"
				 "Please see the file \"AUTHORS\" for a list of contributors",
	.functions = &plugin_info_funcs,
	.data = &plugin_info_data,
};

PLUGIN_INFO(snd_render, default)
{
	qfZoneScoped (true);
	return &plugin_info;
}
