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

#include "snd_internal.h"

static cvar_t  *snd_output;
static cvar_t  *snd_render;
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

	if (!*snd_output->string || !*snd_render->string) {
		Sys_Printf ("Not loading sound due to no renderer/output\n");
		return;
	}

	Sys_RegisterShutdown (S_shutdown, 0);

	PI_RegisterPlugins (snd_output_list);
	PI_RegisterPlugins (snd_render_list);
	snd_output_module = PI_LoadPlugin ("snd_output", snd_output->string);
	if (!snd_output_module) {
		Sys_Printf ("Loading of sound output module: %s failed!\n",
					snd_output->string);
	} else {
		snd_render_module = PI_LoadPlugin ("snd_render", snd_render->string);
		if (!snd_render_module) {
			Sys_Printf ("Loading of sound render module: %s failed!\n",
						snd_render->string);
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
	snd_output = Cvar_Get ("snd_output", SND_OUTPUT_DEFAULT, CVAR_ROM, NULL,
						   "Sound Output Plugin to use");
	snd_render = Cvar_Get ("snd_render", "default", CVAR_ROM, NULL,
						   "Sound Renderer Plugin to use");
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
S_StaticSound (sfx_t *sfx, const vec3_t origin, float vol, float attenuation)
{
	if (snd_render_funcs)
		snd_render_funcs->static_sound (sfx, origin, vol, attenuation);
}

VISIBLE void
S_StartSound (int entnum, int entchannel, sfx_t *sfx, const vec3_t origin,
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
S_Update (struct transform_s *ear, const byte *ambient_sound_level)
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

VISIBLE struct channel_s *
S_AllocChannel (void)
{
	if (snd_render_funcs)
		return snd_render_funcs->alloc_channel ();
	return 0;
}

VISIBLE void
S_ChannelStop (struct channel_s *chan)
{
	if (snd_render_funcs)
		snd_render_funcs->channel_stop (chan);
}
