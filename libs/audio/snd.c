/*
	snd_plugin.c

	sound plugin wrapper

	Copyright (C) 1996-1997  Id Software, Inc.
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

#include "QF/cvar.h"
#include "QF/sound.h"
#include "QF/qargs.h"
#include "QF/sys.h"

#include "QF/scene/transform.h"

#include "snd_internal.h"

static char *snd_output;
static cvar_t snd_output_cvar = {
	.name = "snd_output",
	.description =
		"Sound Output Plugin to use",
	.default_value = SND_OUTPUT_DEFAULT,
	.flags = CVAR_ROM,
	.value = { .type = 0, .value = &snd_output },
};
static char *snd_render;
static cvar_t snd_render_cvar = {
	.name = "snd_render",
	.description =
		"Sound Renderer Plugin to use",
	.default_value = "default",
	.flags = CVAR_ROM,
	.value = { .type = 0, .value = &snd_render },
};
static plugin_t *snd_render_module = NULL;
static plugin_t *snd_output_module = NULL;
static snd_render_funcs_t *snd_render_funcs = NULL;

SND_OUTPUT_PLUGIN_PROTOS
static plugin_list_t snd_output_list[] = {
	SND_OUTPUT_PLUGIN_LIST
};

SND_RENDER_PLUGIN_PROTOS
static plugin_list_t snd_render_list[] = {
	SND_RENDER_PLUGIN_LIST
};

static void
S_shutdown (void *data)
{
	if (snd_render_module) {
		PI_UnloadPlugin (snd_render_module);
		snd_render_module = NULL;
		snd_render_funcs = NULL;
	}
	if (snd_output_module) {
		PI_UnloadPlugin (snd_output_module);
		snd_output_module = NULL;
	}
}

VISIBLE void
S_Init (int *viewentity, double *host_frametime)
{
	if (COM_CheckParm ("-nosound"))
		return;

	if (!*snd_output || !*snd_render) {
		Sys_Printf ("Not loading sound due to no renderer/output\n");
		return;
	}

	Sys_RegisterShutdown (S_shutdown, 0);

	PI_RegisterPlugins (snd_output_list);
	PI_RegisterPlugins (snd_render_list);
	snd_output_module = PI_LoadPlugin ("snd_output", snd_output);
	if (!snd_output_module) {
		Sys_Printf ("Loading of sound output module: %s failed!\n",
					snd_output);
	} else {
		snd_render_module = PI_LoadPlugin ("snd_render", snd_render);
		if (!snd_render_module) {
			Sys_Printf ("Loading of sound render module: %s failed!\n",
						snd_render);
			PI_UnloadPlugin (snd_output_module);
			snd_output_module = NULL;
		} else {
			snd_render_module->data->snd_render->viewentity = viewentity;
			snd_render_module->data->snd_render->host_frametime =
				host_frametime;
			snd_render_module->data->snd_render->output = snd_output_module;

			snd_render_module->functions->snd_render->init ();

			snd_output_module->data->snd_output->soundtime
				= snd_render_module->data->snd_render->soundtime;
			snd_output_module->data->snd_output->paintedtime
				= snd_render_module->data->snd_render->paintedtime;

			snd_render_funcs = snd_render_module->functions->snd_render;
		}
	}
}

VISIBLE void
S_Init_Cvars (void)
{
	Cvar_Register (&snd_output_cvar, 0, 0);
	Cvar_Register (&snd_render_cvar, 0, 0);
}

VISIBLE void
S_AmbientOff (void)
{
	if (snd_render_funcs)
		snd_render_funcs->ambient_off ();
}

VISIBLE void
S_AmbientOn (void)
{
	if (snd_render_funcs)
		snd_render_funcs->ambient_on ();
}

VISIBLE void
S_SetAmbient (int amb_channel, sfx_t *sfx)
{
	if (snd_render_funcs)
		snd_render_funcs->set_ambient (amb_channel, sfx);
}

VISIBLE void
S_StaticSound (sfx_t *sfx, vec4f_t origin, float vol, float attenuation)
{
	if (snd_render_funcs)
		snd_render_funcs->static_sound (sfx, origin, vol, attenuation);
}

VISIBLE void
S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec4f_t origin,
			  float vol, float attenuation)
{
	if (snd_render_funcs)
		snd_render_funcs->start_sound (entnum, entchannel, sfx, origin, vol,
										 attenuation);
}

VISIBLE void
S_StopSound (int entnum, int entchannel)
{
	if (snd_render_funcs)
		snd_render_funcs->stop_sound (entnum, entchannel);
}

VISIBLE sfx_t *
S_PrecacheSound (const char *sample)
{
	if (snd_render_funcs)
		return snd_render_funcs->precache_sound (sample);
	return NULL;
}

VISIBLE void
S_Update (transform_t ear, const byte *ambient_sound_level)
{
	if (snd_render_funcs)
		snd_render_funcs->update (ear, ambient_sound_level);
}

VISIBLE void
S_StopAllSounds (void)
{
	if (snd_render_funcs)
		snd_render_funcs->stop_all_sounds ();
}

VISIBLE void
S_ExtraUpdate (void)
{
	if (snd_render_funcs && snd_render_funcs->extra_update)
		snd_render_funcs->extra_update ();
}

VISIBLE void
S_LocalSound (const char *s)
{
	if (snd_render_funcs)
		snd_render_funcs->local_sound (s);
}

VISIBLE void
S_BlockSound (void)
{
	if (snd_render_funcs)
		snd_render_funcs->block_sound ();
}

VISIBLE void
S_UnblockSound (void)
{
	if (snd_render_funcs)
		snd_render_funcs->unblock_sound ();
}

VISIBLE sfx_t *
S_LoadSound (const char *name)
{
	if (snd_render_funcs)
		return snd_render_funcs->load_sound (name);
	return 0;
}

VISIBLE channel_t *
S_AllocChannel (void)
{
	if (snd_render_funcs)
		return snd_render_funcs->alloc_channel ();
	return 0;
}

VISIBLE void
S_ChannelFree (channel_t *chan)
{
	if (snd_render_funcs) {
		snd_render_funcs->channel_free (chan);
	}
}

VISIBLE int
S_ChannelSetSfx (channel_t *chan, sfx_t *sfx)
{
	if (snd_render_funcs) {
		return snd_render_funcs->channel_set_sfx (chan, sfx);
	}
	return 0;
}

VISIBLE void
S_ChannelSetPaused (channel_t *chan, int paused)
{
	if (snd_render_funcs) {
		snd_render_funcs->channel_set_paused (chan, paused);
	}
}

VISIBLE void
S_ChannelSetLooping (channel_t *chan, int looping)
{
	if (snd_render_funcs) {
		snd_render_funcs->channel_set_looping (chan, looping);
	}
}

VISIBLE chan_state
S_ChannelGetState (channel_t *chan)
{
	if (snd_render_funcs) {
		return snd_render_funcs->channel_get_state (chan);
	}
	return 0;
}

VISIBLE void
S_ChannelSetVolume (channel_t *chan, float volume)
{
	if (snd_render_funcs) {
		snd_render_funcs->channel_set_volume (chan, volume);
	}
}
