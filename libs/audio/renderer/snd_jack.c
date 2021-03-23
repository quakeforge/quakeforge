/*
	snd_jack.c

	JACK version of the renderer

	Copyright (C) 2007 Bill Currie <bill@taniwha.org>

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
#include <stdlib.h>
#include <jack/jack.h>

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/plugin.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "snd_internal.h"

static int sound_started = 0;
static int snd_blocked = 0;
static int snd_shutdown = 0;
static int snd_alive = 0;
static double snd_alive_time = 0;
static jack_client_t *jack_handle;
static jack_port_t *jack_out[2];
static dma_t    _snd_shm;
static float   *output[2];
static cvar_t  *snd_jack_server;

static void s_jack_connect (void);

static void
s_stop_all_sounds (void)
{
	if (!sound_started)
		return;
	SND_StopAllSounds ();
	SND_ScanChannels (1);
}

static void
s_start_sound (int entnum, int entchannel, sfx_t *sfx, const vec3_t origin,
			   float fvol, float attenuation)
{
	if (!sound_started)
		return;
	if (!snd_shutdown)
		SND_StartSound (entnum, entchannel, sfx, origin, fvol, attenuation);
}

static void
s_finish_channels (void)
{
	int         i;
	channel_t  *ch;

	for (i = 0; i < MAX_CHANNELS; i++) {
		ch = &snd_channels[i];
		ch->done = ch->stop = 1;
	}
}

static void
s_update (const vec3_t origin, const vec3_t forward, const vec3_t right,
		  const vec3_t up, const byte *ambient_sound_level)
{
	double      now = Sys_DoubleTime ();

	if (!sound_started)
		return;
	if (snd_alive) {
		snd_alive = 0;
		snd_alive_time = now;
	} else {
		if (!snd_shutdown) {
			if (now - snd_alive_time > 1.0) {
				Sys_Printf ("jackd client thread seems to have died\n");
				s_finish_channels ();
				snd_shutdown = 1;
			}
		}
	}
	if (snd_shutdown) {
		if (snd_shutdown == 1) {
			snd_shutdown++;
			Sys_Printf ("Lost connection to jackd\n");
			s_jack_connect ();
		}
		if (snd_shutdown)
			return;
	}
	SND_SetListener (origin, forward, right, up, ambient_sound_level);
}

static void
s_extra_update (void)
{
}

static void
s_local_sound (const char *sound)
{
	if (!sound_started)
		return;
	if (!snd_shutdown)
		SND_LocalSound (sound);
}

static void
s_jack_activate (void)
{
	const char **ports;
	int         i;

	if (jack_activate (jack_handle)) {
		Sys_Printf ("Could not activate JACK\n");
		return;
	}
	snd_shutdown = 0;
	ports = jack_get_ports (jack_handle, 0, 0,
							JackPortIsPhysical | JackPortIsInput);
	if (developer->int_val & SYS_SND) {
		for (i = 0; ports[i]; i++)
			Sys_Printf ("%s\n", ports[i]);
	}
	for (i = 0; i < 2; i++)
		jack_connect (jack_handle, jack_port_name (jack_out[i]), ports[i]);
	free (ports);
}

static void
s_block_sound (void)
{
	if (!sound_started)
		return;
	if (++snd_blocked == 1) {
		//Sys_Printf ("jack_deactivate: %d\n", jack_deactivate (jack_handle));
	}
}

static void
s_unblock_sound (void)
{
	if (!sound_started)
		return;
	if (!snd_blocked)
		return;

	if (!--snd_blocked) {
		//s_jack_activate ();
		//Sys_Printf ("jack_activate\n");
	}
}

static channel_t *
s_alloc_channel (void)
{
	if (!sound_started)
		return 0;
	if (!snd_shutdown)
		return SND_AllocChannel ();
	return 0;
}

static void
s_snd_force_unblock (void)
{
	if (!sound_started)
		return;
	snd_blocked = 1;
	s_unblock_sound ();
}

static void
s_ambient_off (void)
{
	if (!sound_started)
		return;
	SND_AmbientOff ();
}

static void
s_ambient_on (void)
{
	if (!sound_started)
		return;
	SND_AmbientOn ();
}

static void
s_static_sound (sfx_t *sfx, const vec3_t origin, float vol,
				float attenuation)
{
	if (!sound_started)
		return;
	SND_StaticSound (sfx, origin, vol, attenuation);
}

static void
s_stop_sound (int entnum, int entchannel)
{
	if (!sound_started)
		return;
	SND_StopSound (entnum, entchannel);
}

static sfx_t *
s_precache_sound (const char *name)
{
	if (!sound_started)
		return 0;
	return SND_PrecacheSound (name);
}

static sfx_t *
s_load_sound (const char *name)
{
	if (!sound_started)
		return 0;
	return SND_LoadSound (name);
}

static void
s_channel_stop (channel_t *chan)
{
	if (!sound_started)
		return;
	SND_ChannelStop (chan);
}

static void
snd_jack_xfer (portable_samplepair_t *paintbuffer, int count, float volume)
{
	int         i;

	if (snd_blocked) {
		for (i = 0; i < count; i++) {
			*output[0]++ = 0;
			*output[1]++ = 0;
		}
		return;
	}
	for (i = 0; i < count; i++) {
		/* max is +/- 1.0. need to implement clamping. */
		*output[0]++ = volume * snd_paintbuffer[i].left;
		*output[1]++ = volume * snd_paintbuffer[i].right;
	}
}

static int
snd_jack_process (jack_nframes_t nframes, void *arg)
{
	int         i;

	snd_alive = 1;
	for (i = 0; i < 2; i++)
		output[i] = (float *) jack_port_get_buffer (jack_out[i], nframes);
	SND_PaintChannels (snd_paintedtime + nframes);
	return 0;
}

static void
snd_jack_shutdown (void *arg)
{
	snd_shutdown = 1;
	s_finish_channels ();
}

static void
snd_jack_error (const char *desc)
{
	fprintf (stderr, "snd_jack: %s\n", desc);
}

static int
snd_jack_xrun (void *arg)
{
	if (developer->int_val & SYS_SND) {
		fprintf (stderr, "snd_jack: xrun\n");
	}
	return 0;
}

static void
s_jack_connect (void)
{
	int         i;

	jack_set_error_function (snd_jack_error);
	if ((jack_handle = jack_client_open ("QuakeForge",
										 JackServerName | JackNoStartServer, 0,
										 snd_jack_server->string)) == 0) {
		Sys_Printf ("Could not connect to JACK\n");
		return;
	}
	if (jack_set_xrun_callback (jack_handle, snd_jack_xrun, 0))
		Sys_Printf ("Could not set xrun callback\n");
	jack_set_process_callback (jack_handle, snd_jack_process, 0);
	jack_on_shutdown (jack_handle, snd_jack_shutdown, 0);
	for (i = 0; i < 2; i++)
		jack_out[i] = jack_port_register (jack_handle, va (0, "out_%d", i + 1),
										  JACK_DEFAULT_AUDIO_TYPE,
										  JackPortIsOutput, 0);
	snd_shm->speed = jack_get_sample_rate (jack_handle);
	s_jack_activate ();
	sound_started = 1;
	Sys_Printf ("Connected to JACK: %d Sps\n", snd_shm->speed);
}

static void
s_init (void)
{
	snd_shm = &_snd_shm;
	snd_shm->xfer = snd_jack_xfer;

	Cmd_AddCommand ("snd_force_unblock", s_snd_force_unblock,
					"fix permanently blocked sound");

	snd_volume = Cvar_Get ("volume", "0.7", CVAR_ARCHIVE, NULL,
						   "Set the volume for sound playback");
	snd_jack_server = Cvar_Get ("snd_jack_server", "default", CVAR_ROM, NULL,
								"The name of the JACK server to connect to");

	SND_SFX_Init ();
	SND_Channels_Init ();

	s_jack_connect ();
}

static void
s_shutdown (void)
{
	int         i;
	if (jack_handle) {
		jack_deactivate (jack_handle);
		if (!snd_shutdown) {
			for (i = 0; i < 2; i++)
				jack_port_unregister (jack_handle, jack_out[i]);
		}
		jack_client_close (jack_handle);
	}
}

static general_funcs_t plugin_info_general_funcs = {
	s_init,
	s_shutdown,
};

static snd_render_funcs_t plugin_info_render_funcs = {
	s_ambient_off,
	s_ambient_on,
	s_static_sound,
	s_start_sound,
	s_stop_sound,
	s_precache_sound,
	s_update,
	s_stop_all_sounds,
	s_extra_update,
	s_local_sound,
	s_block_sound,
	s_unblock_sound,
	s_load_sound,
	s_alloc_channel,
	s_channel_stop,
};

static general_data_t plugin_info_general_data;

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
	"Copyright (C) 2007  contributors of the QuakeForge "
	"project\n"
	"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(snd_render, jack)
{
	return &plugin_info;
}
