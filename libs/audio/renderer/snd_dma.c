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

#include "winquake.h"

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

#include "snd_internal.h"

static qboolean snd_initialized = false;
static int      snd_blocked = 0;

static unsigned soundtime;					// sample PAIRS

static int      sound_started = 0;


static cvar_t  *nosound;
static cvar_t  *snd_mixahead;
static cvar_t  *snd_noextraupdate;
static cvar_t  *snd_show;

static general_data_t plugin_info_general_data;

static snd_output_funcs_t *snd_output_funcs;
static snd_output_data_t *snd_output_data;

static snd_t    snd = {
	.finish_channels = SND_FinishChannels,
	.paint_channels  = SND_PaintChannels,
};
static int      snd_shutdown = 0;

static void
s_xfer_paint_buffer (snd_t *snd, portable_samplepair_t *paintbuffer, int count,
					 float volume)
{
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
	int			clear, i;
	int         count;

	if (!sound_started || !snd || !snd->buffer)
		return;

	if (snd->samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	count = snd->frames * snd->channels * snd->samplebits / 8;
	for (i = 0; i < count; i++)
		snd->buffer[i] = clear;
}

static void
s_stop_all_sounds (void)
{
	SND_StopAllSounds (&snd);
	SND_ScanChannels (&snd, 0);
	s_clear_buffer (&snd);
}

//=============================================================================

static void
s_get_soundtime (void)
{
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
	endtime = soundtime + snd_mixahead->value * snd.speed;
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
s_update (struct transform_s *ear, const byte *ambient_sound_level)
{
	if (!sound_started || (snd_blocked > 0))
		return;

	if (snd_output_funcs->on_update) {
		snd_output_funcs->on_update (&snd);
	}

	SND_SetListener (&snd, ear, ambient_sound_level);

	if (snd_output_data->model == som_push) {
		// mix some sound
		s_update_ ();
		SND_ScanChannels (&snd, 0);
	}
}

static void
s_extra_update (void)
{
	if (snd_output_data->model == som_push) {
		if (!sound_started || snd_noextraupdate->int_val)
			return;							// don't pollute timings
		s_update_ ();
	}
}

static void
s_block_sound (void)
{
	if (++snd_blocked == 1) {
		snd_output_funcs->block_sound (&snd);
		s_clear_buffer (&snd);
	}
}

static void
s_unblock_sound (void)
{
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
	s_stop_all_sounds ();
}

static void
s_startup (void)
{
	if (!snd_initialized)
		return;

	if (!snd_output_funcs->init (&snd)) {
		Sys_Printf ("S_Startup: S_O_Init failed.\n");
		sound_started = 0;
		return;
	}
	if (!snd.xfer)
		snd.xfer = s_xfer_paint_buffer;

	sound_started = 1;
}

static void
s_snd_force_unblock (void)
{
	snd_blocked = 1;
	s_unblock_sound ();
}

static void
s_init_cvars (void)
{
	nosound = Cvar_Get ("nosound", "0", CVAR_NONE, NULL,
						"Set to turn sound off");
	snd_volume = Cvar_Get ("volume", "0.7", CVAR_ARCHIVE, NULL,
						   "Set the volume for sound playback");
	snd_mixahead = Cvar_Get ("snd_mixahead", "0.1", CVAR_ARCHIVE, NULL,
							  "Delay time for sounds");
	snd_noextraupdate = Cvar_Get ("snd_noextraupdate", "0", CVAR_NONE, NULL,
								  "Toggles the correct value display in "
								  "host_speeds. Usually messes up sound "
								  "playback when in effect");
	snd_show = Cvar_Get ("snd_show", "0", CVAR_NONE, NULL,
						 "Toggles display of sounds currently being played");
}

static void
s_init (void)
{
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

// FIXME
//	if (host_parms.memsize < 0x800000) {
//		Cvar_Set (snd_loadas8bit, "1");
//		Sys_Printf ("loading all sounds as 8bit\n");
//	}

	snd_initialized = true;

	s_startup ();

	if (sound_started == 0)				// sound startup failed? Bail out.
		return;

	SND_SFX_Init (&snd);
	SND_Channels_Init (&snd);

	s_stop_all_sounds ();
}

static void
s_shutdown (void)
{
	if (!sound_started)
		return;

	sound_started = 0;
	snd_shutdown = 1;

	snd_output_funcs->shutdown (&snd);
}

static void
s_ambient_off (void)
{
	if (!sound_started)
		return;
	SND_AmbientOff (&snd);
}

static void
s_ambient_on (void)
{
	if (!sound_started)
		return;
	SND_AmbientOn (&snd);
}

static void
s_static_sound (sfx_t *sfx, vec4f_t origin, float vol,
				float attenuation)
{
	if (!sound_started)
		return;
	SND_StaticSound (&snd, sfx, origin, vol, attenuation);
}

static void
s_start_sound (int entnum, int entchannel, sfx_t *sfx, vec4f_t origin,
			   float vol, float attenuation)
{
	if (!sound_started)
		return;
	if (!snd_shutdown)
		SND_StartSound (&snd, entnum, entchannel, sfx, origin, vol,
						attenuation);
}

static void
s_stop_sound (int entnum, int entchannel)
{
	if (!sound_started)
		return;
	SND_StopSound (&snd, entnum, entchannel);
}

static sfx_t *
s_precache_sound (const char *name)
{
	if (!sound_started)
		return 0;
	return SND_PrecacheSound (&snd, name);
}

static sfx_t *
s_load_sound (const char *name)
{
	if (!sound_started)
		return 0;
	return SND_LoadSound (&snd, name);
}

static void
s_channel_stop (channel_t *chan)
{
	if (!sound_started)
		return;
	SND_ChannelStop (&snd, chan);
}

static void
s_local_sound (const char *sound)
{
	if (!sound_started)
		return;
	if (!snd_shutdown)
		SND_LocalSound (&snd, sound);
}

static channel_t *
s_alloc_channel (void)
{
	if (!sound_started)
		return 0;
	if (!snd_shutdown)
		return SND_AllocChannel (&snd);
	return 0;
}

static general_funcs_t plugin_info_general_funcs = {
	.init = s_init_cvars,
	.shutdown = s_shutdown,
};

static snd_render_funcs_t plugin_info_render_funcs = {
	.init = s_init,
	.ambient_off     = s_ambient_off,
	.ambient_on      = s_ambient_on,
	.static_sound    = s_static_sound,
	.start_sound     = s_start_sound,
	.local_sound     = s_local_sound,
	.stop_sound      = s_stop_sound,

	.alloc_channel   = s_alloc_channel,
	.channel_stop    = s_channel_stop,

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
	qfp_snd_render,
	0,
	QFPLUGIN_VERSION,
	"0.1",
	"Sound Renderer",
	"Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001  contributors of the QuakeForge "
		"project\n"
		"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(snd_render, default)
{
	return &plugin_info;
}
