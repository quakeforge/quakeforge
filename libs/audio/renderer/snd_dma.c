/*
	snd_dma.c

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

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

#include "snd_render.h"

unsigned int    snd_paintedtime;				// sample PAIRS

cvar_t         *snd_loadas8bit;
cvar_t         *snd_volume;
cvar_t         *snd_interp;

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

static void
s_clear_buffer (void)
{
	int			clear, i;

	if (!sound_started || !snd_shm || !snd_shm->buffer)
		return;

	if (snd_shm->samplebits == 8)
		clear = 0x80;
	else
		clear = 0;

	for (i = 0; i < snd_shm->samples * snd_shm->samplebits / 8; i++)
		snd_shm->buffer[i] = clear;
}

static void
s_stop_all_sounds (void)
{
	SND_StopAllSounds ();
	s_clear_buffer ();
}

//=============================================================================

static void
s_get_soundtime (void)
{
	int			fullsamples, samplepos;
	static int	buffers, oldsamplepos;

	fullsamples = snd_shm->samples / snd_shm->channels;

	// it is possible to miscount buffers if it has wrapped twice between
	// calls to s_update.  Oh well.
	if ((samplepos = snd_output_funcs->pS_O_GetDMAPos ()) == -1)
		return;
	
	if (samplepos < oldsamplepos) {
		buffers++;						// buffer wrapped

		if (snd_paintedtime > 0x40000000) {	// time to chop things off to avoid
			// 32 bit limits
			buffers = 0;
			snd_paintedtime = fullsamples;
			s_stop_all_sounds ();
		}
	}
	oldsamplepos = samplepos;

	soundtime = buffers * fullsamples + samplepos / snd_shm->channels;
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
	if (snd_paintedtime < soundtime) {
//		Sys_Printf ("S_Update_ : overflow\n");
		snd_paintedtime = soundtime;
	}
	// mix ahead of current position
	endtime = soundtime + snd_mixahead->value * snd_shm->speed;
	samps = snd_shm->samples >> (snd_shm->channels - 1);
	if (endtime - soundtime > samps)
		endtime = soundtime + samps;

	SND_PaintChannels (endtime);
	snd_output_funcs->pS_O_Submit ();
}

/*
	s_update

	Called once each time through the main loop
*/
static void
s_update (const vec3_t origin, const vec3_t forward, const vec3_t right,
			const vec3_t up)
{
	if (!sound_started || (snd_blocked > 0))
		return;

	SND_SetListener (origin, forward, right, up);

	// mix some sound
	s_update_ ();
}

static void
s_extra_update (void)
{
	if (!sound_started || snd_noextraupdate->int_val)
		return;							// don't pollute timings
	s_update_ ();
}

static void
s_block_sound (void)
{
	if (++snd_blocked == 1) {
		snd_output_funcs->pS_O_BlockSound ();
		s_clear_buffer ();
	}
}

static void
s_unblock_sound (void)
{
	if (!snd_blocked)
		return;

	if (!--snd_blocked) {
		s_clear_buffer ();
		snd_output_funcs->pS_O_UnblockSound ();
	}
}

/* console functions */

static void
s_soundinfo_f (void)
{
	if (!sound_started || !snd_shm) {
		Sys_Printf ("sound system not started\n");
		return;
	}

	Sys_Printf ("%5d stereo\n", snd_shm->channels - 1);
	Sys_Printf ("%5d samples\n", snd_shm->samples);
	Sys_Printf ("%5d samplepos\n", snd_shm->samplepos);
	Sys_Printf ("%5d samplebits\n", snd_shm->samplebits);
	Sys_Printf ("%5d submission_chunk\n", snd_shm->submission_chunk);
	Sys_Printf ("%5d speed\n", snd_shm->speed);
	Sys_Printf ("0x%lx dma buffer\n", (unsigned long) snd_shm->buffer);
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

	snd_shm = snd_output_funcs->pS_O_Init ();

	if (!snd_shm) {
		Sys_Printf ("S_Startup: S_O_Init failed.\n");
		sound_started = 0;
		return;
	}

	sound_started = 1;
}

static void
s_init (void)
{
	snd_output_funcs = snd_render_data.output->functions->snd_output;
	snd_render_data.soundtime = &soundtime;
	Sys_Printf ("\nSound Initialization\n");

	Cmd_AddCommand ("stopsound", s_stop_all_sounds_f,
					"Stops all sounds currently being played");
	Cmd_AddCommand ("soundinfo", s_soundinfo_f,
					"Report information on the sound system");

	snd_interp = Cvar_Get ("snd_interp", "1", CVAR_ARCHIVE, NULL,
	                              "control sample interpolation");
	nosound = Cvar_Get ("nosound", "0", CVAR_NONE, NULL,
						"Set to turn sound off");
	snd_volume = Cvar_Get ("volume", "0.7", CVAR_ARCHIVE, NULL,
						   "Set the volume for sound playback");
	snd_interp = Cvar_Get ("snd_interp", "1", CVAR_ARCHIVE, NULL,
						   "control sample interpolation");
	snd_loadas8bit = Cvar_Get ("snd_loadas8bit", "0", CVAR_NONE, NULL,
							   "Toggles loading sounds as 8-bit samples");
	snd_mixahead = Cvar_Get ("snd_mixahead", "0.1", CVAR_ARCHIVE, NULL,
							  "Delay time for sounds");
	snd_noextraupdate = Cvar_Get ("snd_noextraupdate", "0", CVAR_NONE, NULL,
								  "Toggles the correct value display in "
								  "host_speeds. Usually messes up sound "
								  "playback when in effect");
	snd_show = Cvar_Get ("snd_show", "0", CVAR_NONE, NULL,
						 "Toggles display of sounds currently being played");
// FIXME
//	if (host_parms.memsize < 0x800000) {
//		Cvar_Set (snd_loadas8bit, "1");
//		Sys_Printf ("loading all sounds as 8bit\n");
//	}

	snd_initialized = true;

	s_startup ();

	if (sound_started == 0)				// sound startup failed? Bail out.
		return;

	SND_SFX_Init ();
	SND_Channels_Init ();

	SND_InitScaletable ();

	s_stop_all_sounds ();
}

static void
s_shutdown (void)
{
	if (!sound_started)
		return;

	sound_started = 0;

	snd_output_funcs->pS_O_Shutdown ();

	snd_shm = 0;
}

static general_funcs_t plugin_info_general_funcs = {
	s_init,
	s_shutdown,
};

static snd_render_funcs_t plugin_info_render_funcs = {
	SND_AmbientOff,
	SND_AmbientOn,
	SND_TouchSound,
	SND_StaticSound,
	SND_StartSound,
	SND_StopSound,
	SND_PrecacheSound,
	s_update,
	s_stop_all_sounds,
	s_extra_update,
	SND_LocalSound,
	s_block_sound,
	s_unblock_sound,
	SND_LoadSound,
	SND_AllocChannel,
};

static plugin_funcs_t plugin_info_funcs = {
	&plugin_info_general_funcs,
	0,
	0,
	0,
	0,
	&plugin_info_render_funcs,
};

static plugin_data_t plugin_info_data = {
	&plugin_info_general_data,
	0,
	0,
	0,
	0,
	&snd_render_data,
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
