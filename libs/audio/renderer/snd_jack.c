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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id: snd_dma.c 11380 2007-03-10 14:17:52Z taniwha $";

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

#include "snd_render.h"

static int snd_blocked = 0;
static int snd_shutdown = 0;
static jack_client_t *jack_handle;
static jack_port_t *jack_out[2];
static dma_t    _snd_shm;
static float   *output[2];
static cvar_t  *snd_jack_server;

static void
s_stop_all_sounds (void)
{
	SND_StopAllSounds ();
	SND_ScanChannels (1);
}

static void
s_update (const vec3_t origin, const vec3_t forward, const vec3_t right,
		  const vec3_t up)
{
	if (snd_shutdown) {
		if (snd_shutdown == 1) {
			snd_shutdown++;
			Sys_Printf ("Lost connection to jackd\n");
		}
		return;
	}
	SND_SetListener (origin, forward, right, up);
}

static void
s_extra_update (void)
{
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
	ports = jack_get_ports (jack_handle, 0, 0,
							JackPortIsPhysical | JackPortIsInput);
	//for (i = 0; ports[i]; i++)
	//	Sys_Printf ("%s\n", ports[i]);
	for (i = 0; i < 2; i++)
		jack_connect (jack_handle, jack_port_name (jack_out[i]), ports[i]);
	free (ports);
}

static void
s_block_sound (void)
{
	if (++snd_blocked == 1) {
		//Sys_Printf ("jack_deactivate: %d\n", jack_deactivate (jack_handle));
	}
}

static void
s_unblock_sound (void)
{
	if (!snd_blocked)
		return;

	if (!--snd_blocked) {
		//s_jack_activate ();
		//Sys_Printf ("jack_activate\n");
	}
}

static void
s_snd_force_unblock (void)
{
	snd_blocked = 1;
	s_unblock_sound ();
}

static void
snd_jack_xfer (int endtime)
{
	int         i;
	int         count;

	count = endtime - snd_paintedtime;
	if (snd_blocked) {
		for (i = 0; i < count; i++) {
			*output[0]++ = 0;
			*output[1]++ = 0;
		}
		return;
	}
	for (i = 0; i < count; i++) {
		*output[0]++ = 0.25 * snd_paintbuffer[i].left / 65536.0;
		*output[1]++ = 0.25 * snd_paintbuffer[i].right / 65536.0;
	}
}

static int
snd_jack_process (jack_nframes_t nframes, void *arg)
{
	int         i;

	for (i = 0; i < 2; i++)
		output[i] = (float *) jack_port_get_buffer (jack_out[i], nframes);
	SND_PaintChannels (snd_paintedtime + nframes);
	return 0;
}

static void
snd_jack_shutdown (void *arg)
{
	int         i;
	channel_t  *ch;
	snd_shutdown = 1;
	for (i = 0; i < MAX_CHANNELS; i++) {
		ch = &snd_channels[i];
		ch->done = ch->stop = 1;
	}
}

static void
s_init (void)
{
	int         i;

	snd_shm = &_snd_shm;
	snd_shm->xfer = snd_jack_xfer;

	Cmd_AddCommand ("snd_force_unblock", s_snd_force_unblock,
					"fix permanently blocked sound");

	snd_volume = Cvar_Get ("volume", "0.7", CVAR_ARCHIVE, NULL,
						   "Set the volume for sound playback");
	snd_interp = Cvar_Get ("snd_interp", "1", CVAR_ARCHIVE, NULL,
						   "control sample interpolation");
	snd_loadas8bit = Cvar_Get ("snd_loadas8bit", "0", CVAR_NONE, NULL,
							   "Toggles loading sounds as 8-bit samples");
	snd_jack_server = Cvar_Get ("snd_jack_server", "default", CVAR_ROM, NULL,
								"The name of the JACK server to connect to");

	SND_SFX_Init ();
	SND_Channels_Init ();

	if ((jack_handle = jack_client_open ("QuakeForge",
										 JackServerName | JackNoStartServer, 0,
										 snd_jack_server->string)) == 0) {
		Sys_Printf ("Could not connect to JACK\n");
		return;
	}
	jack_set_process_callback (jack_handle, snd_jack_process, 0);
	jack_on_shutdown (jack_handle, snd_jack_shutdown, 0);
	for (i = 0; i < 2; i++)
		jack_out[i] = jack_port_register (jack_handle, va ("out_%d", i + 1),
										  JACK_DEFAULT_AUDIO_TYPE,
										  JackPortIsOutput, 0);
	snd_shm->speed = jack_get_sample_rate (jack_handle);
	s_jack_activate ();
	Sys_Printf ("Connected to JACK: %d Sps\n", snd_shm->speed);
	if (snd_shm->speed > 44100) {
		Sys_Printf ("FIXME clamping Sps to 44100 until resampling is fixed\n");
		snd_shm->speed = 44100;
	}
}

static void
s_shutdown (void)
{
	int         i;
	if (jack_handle) {
		jack_deactivate (jack_handle);
		for (i = 0; i < 2; i++)
			jack_port_unregister (jack_handle, jack_out[i]);
		jack_client_close (jack_handle);
	}
}

static general_funcs_t plugin_info_general_funcs = {
	s_init,
	s_shutdown,
};

static snd_render_funcs_t plugin_info_render_funcs = {
	SND_AmbientOff,
	SND_AmbientOn,
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
	SND_ChannelStop,
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
