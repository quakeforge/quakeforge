/*
	snd_jack.c

	JACK output driver

	Copyright (C) 2007-2021 Bill Currie <bill@taniwha.org>

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
static float   *output[2];
static cvar_t  *snd_jack_server;
static cvar_t  *snd_jack_ports;

static int s_jack_connect (snd_t *snd);

static void
s_update (snd_t *snd)
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
				snd->finish_channels ();
				snd_shutdown = 1;
			}
		}
	}
	if (snd_shutdown) {
		if (snd_shutdown == 1) {
			snd_shutdown++;
			Sys_Printf ("Lost connection to jackd\n");
			s_jack_connect (snd);
		}
		if (snd_shutdown)
			return;
	}
}

static const char **
s_jack_parse_ports (const char *ports_str)
{
	int         num_ports = 1;
	int         ind = 0;
	char      **ports;

	for (const char *s = ports_str; *s; s++) {
		num_ports += *s == ';';
	}

	size_t      len = strlen (ports_str) + 1;
	ports = malloc ((num_ports + 1) * sizeof (*ports) + len + 1);
	ports[num_ports] = 0;
	ports[0] = (char *) &ports[num_ports + 1];
	ports[0][len] = 0;
	strcpy (ports[0], ports_str);

	for (char *s = ports[0]; *s; s++) {
		if (s > ports[0]) {
			s[-1] = 0;
		}
		for (ports[ind++] = s; *s && *s != ';'; s++) {
		}
	}

	return (const char **) ports;
}

static void
s_jack_activate (void)
{
	const char **ports;
	int         i;

	snd_shutdown = 0;
	if (!*snd_jack_ports->string) {
		ports = jack_get_ports (jack_handle, 0, 0,
								JackPortIsPhysical | JackPortIsInput);
	} else {
		ports = s_jack_parse_ports (snd_jack_ports->string);
	}
	if (developer->int_val & SYS_snd) {
		for (i = 0; ports[i]; i++) {
			Sys_Printf ("%s\n", ports[i]);
		}
	}
	if (jack_activate (jack_handle)) {
		Sys_Printf ("Could not activate JACK\n");
		return;
	}
	for (i = 0; i < 2; i++) {
		jack_connect (jack_handle, jack_port_name (jack_out[i]), ports[i]);
	}
	free (ports);
}

static void
s_block_sound (snd_t *t)
{
	if (!sound_started)
		return;
	if (++snd_blocked == 1) {
		//Sys_Printf ("jack_deactivate: %d\n", jack_deactivate (jack_handle));
	}
}

static void
s_unblock_sound (snd_t *t)
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

static void
snd_jack_xfer (snd_t *snd, portable_samplepair_t *paintbuffer, int count,
			   float volume)
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
		*output[0]++ = volume * paintbuffer[i].left;
		*output[1]++ = volume * paintbuffer[i].right;
	}
}

static int
snd_jack_process (jack_nframes_t nframes, void *arg)
{
	snd_t      *snd = arg;
	int         i;

	snd_alive = 1;
	for (i = 0; i < 2; i++)
		output[i] = (float *) jack_port_get_buffer (jack_out[i], nframes);
	snd->paint_channels (snd, snd->paintedtime + nframes);
	return 0;
}

static void
snd_jack_shutdown (void *arg)
{
	snd_t      *snd = arg;

	snd_shutdown = 1;
	snd->finish_channels ();
}

static void
snd_jack_error (const char *desc)
{
	fprintf (stderr, "snd_jack: %s\n", desc);
}

static int
snd_jack_xrun (void *arg)
{
	if (developer->int_val & SYS_snd) {
		fprintf (stderr, "snd_jack: xrun\n");
	}
	return 0;
}

static int
s_jack_connect (snd_t *snd)
{
	int         i;

	jack_set_error_function (snd_jack_error);
	if ((jack_handle = jack_client_open ("QuakeForge",
										 JackServerName | JackNoStartServer, 0,
										 snd_jack_server->string)) == 0) {
		Sys_Printf ("Could not connect to JACK\n");
		return 0;
	}
	if (jack_set_xrun_callback (jack_handle, snd_jack_xrun, 0))
		Sys_Printf ("Could not set xrun callback\n");
	jack_set_process_callback (jack_handle, snd_jack_process, snd);
	jack_on_shutdown (jack_handle, snd_jack_shutdown, snd);
	for (i = 0; i < 2; i++)
		jack_out[i] = jack_port_register (jack_handle, va (0, "out_%d", i + 1),
										  JACK_DEFAULT_AUDIO_TYPE,
										  JackPortIsOutput, 0);
	snd->speed = jack_get_sample_rate (jack_handle);
	snd->channels = 2;
	s_jack_activate ();
	sound_started = 1;
	Sys_Printf ("Connected to JACK: %d Sps\n", snd->speed);
	return 1;
}

static int
s_init (snd_t *snd)
{
	snd->xfer = snd_jack_xfer;
	return s_jack_connect (snd);
}

static void
s_shutdown (snd_t *snd)
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

static void
s_init_cvars (void)
{
	snd_jack_server = Cvar_Get ("snd_jack_server", "default", CVAR_ROM, NULL,
								"The name of the JACK server to connect to");
	snd_jack_ports = Cvar_Get ("snd_jack_ports", "", CVAR_ROM, NULL,
							   "; separated list of port names to which QF "
							   "will connect. Defaults to the first two "
							   "physical ports. Currently only two ports "
							   "are supported.");
}

static general_funcs_t plugin_info_general_funcs = {
	.init = s_init_cvars,
};

static snd_output_funcs_t plugin_info_output_funcs = {
	.init = s_init,
	.shutdown = s_shutdown,
	.on_update = s_update,
	.block_sound = s_block_sound,
	.unblock_sound = s_unblock_sound,
};

static snd_output_data_t plugin_info_output_data = {
	.model = som_pull,
};

static general_data_t plugin_info_general_data;

static plugin_funcs_t plugin_info_funcs = {
	.general = &plugin_info_general_funcs,
	.snd_output = &plugin_info_output_funcs,
};

static plugin_data_t plugin_info_data = {
	.general = &plugin_info_general_data,
	.snd_output = &plugin_info_output_data,
};

static plugin_t plugin_info = {
	qfp_snd_output,
	0,
	QFPLUGIN_VERSION,
	"0.1",
	"Sound Renderer",
	"Copyright (C) 2007-2021 contributors of the QuakeForge "
	"project\n"
	"Please see the file \"AUTHORS\" for a list of contributors",
	&plugin_info_funcs,
	&plugin_info_data,
};

PLUGIN_INFO(snd_output, jack)
{
	return &plugin_info;
}
