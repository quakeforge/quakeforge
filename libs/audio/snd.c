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
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cvar.h"
#include "QF/sound.h"
#include "QF/model.h"
#include "QF/plugin.h"
#include "QF/sys.h"

// =======================================================================
// Various variables also defined in snd_dma.c
// FIXME - should be put in one place
// =======================================================================
channel_t       channels[MAX_CHANNELS];
int             total_channels;
volatile dma_t *shm = 0;
cvar_t         *snd_loadas8bit;
int             paintedtime;				// sample PAIRS
qboolean        snd_initialized = false;

cvar_t         *bgmvolume;
cvar_t         *volume;
cvar_t         *snd_interp;

cvar_t         *snd_output;
cvar_t         *snd_render;
plugin_t       *snd_render_module = NULL;
plugin_t       *snd_output_module = NULL;

SND_OUTPUT_PROTOS
plugin_list_t   snd_output_list[] = {
	SND_OUTPUT_LIST
};

SND_RENDER_PROTOS
plugin_list_t   snd_render_list[] = {
	SND_RENDER_LIST
};


void
S_Init (struct model_s **worldmodel, int *viewentity, double *host_frametime)
{
	S_Init_Cvars ();
	if (!*snd_output->string || !*snd_render->string) {
		Sys_Printf ("Not loading sound due to no renderer/output\n");
		return;
	}

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
			snd_render_module->data->snd_render->worldmodel = worldmodel;
			snd_render_module->data->snd_render->viewentity = viewentity;
			snd_render_module->data->snd_render->host_frametime =
				host_frametime;

			snd_output_module->data->snd_output->soundtime
				= snd_render_module->data->snd_render->soundtime;
			snd_output_module->data->snd_output->paintedtime
				= snd_render_module->data->snd_render->paintedtime;

			snd_output_module->functions->general->p_Init ();
			snd_render_module->functions->general->p_Init ();
		}
	}
}

void
S_Init_Cvars (void)
{
	volume = Cvar_Get ("volume", "0.7", CVAR_ARCHIVE, NULL,
					   "Volume level of sounds");
	snd_loadas8bit = Cvar_Get ("snd_loadas8bit", "0", CVAR_NONE, NULL,
							   "Load samples as 8-bit");
	bgmvolume = Cvar_Get ("bgmvolume", "1", CVAR_ARCHIVE, NULL,
						  "CD music volume");
	snd_interp = Cvar_Get ("snd_interp", "1", CVAR_ARCHIVE, NULL,
	                              "control sample interpolation");
#ifdef _WIN32
	snd_output = Cvar_Get ("snd_output", "sdl", CVAR_ARCHIVE, NULL,
						   "Sound Output Plugin to use");
#else
	snd_output = Cvar_Get ("snd_output", "oss", CVAR_ARCHIVE, NULL,
						   "Sound Output Plugin to use");
#endif
	snd_render = Cvar_Get ("snd_render", "default", CVAR_ARCHIVE, NULL,
						   "Sound Renderer Plugin to use");
}

void
S_AmbientOff (void)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_AmbientOff ();
}

void
S_AmbientOn (void)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_AmbientOn ();
}

void
S_Shutdown (void)
{
	if (snd_render_module) {
		PI_UnloadPlugin (snd_render_module);
		snd_render_module = NULL;
	}
	if (snd_output_module) {
		PI_UnloadPlugin (snd_output_module);
		snd_output_module = NULL;
	}
}

void
S_TouchSound (const char *sample)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_TouchSound (sample);
}

void
S_ClearBuffer (void)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_ClearBuffer ();
}

void
S_StaticSound (sfx_t *sfx, vec3_t origin, float vol, float attenuation)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_StaticSound (sfx, origin, vol, attenuation);
}

void
S_StartSound (int entnum, int entchannel, sfx_t *sfx, vec3_t origin,
			  float fvol, float attenuation)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_StartSound
			(entnum, entchannel, sfx, origin, fvol, attenuation);
}

void
S_StopSound (int entnum, int entchannel)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_StopSound
			(entnum, entchannel);
}

sfx_t      *
S_PrecacheSound (const char *sample)
{
	if (snd_render_module)
		return snd_render_module->functions->snd_render->pS_PrecacheSound
			(sample);
	else
		return NULL;
}

void
S_ClearPrecache (void)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_ClearPrecache ();
}

void
S_Update (vec3_t origin, vec3_t v_forward, vec3_t v_right, vec3_t v_up)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_Update (origin, v_forward,
															 v_right, v_up);
}

void
S_StopAllSounds (qboolean clear)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_StopAllSounds (clear);
}

void
S_BeginPrecaching (void)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_BeginPrecaching ();
}

void
S_EndPrecaching (void)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_EndPrecaching ();
}

void
S_ExtraUpdate (void)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_ExtraUpdate ();
}

void
S_LocalSound (const char *s)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_LocalSound (s);
}

void
S_BlockSound (void)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_BlockSound ();
}

void
S_UnblockSound (void)
{
	if (snd_render_module)
		snd_render_module->functions->snd_render->pS_UnblockSound ();
}


qboolean
S_O_Init (void)
{
	if (snd_output_module)
		return snd_output_module->functions->snd_output->pS_O_Init ();
	else
		return false;
}

void
S_O_Shutdown (void)
{
	if (snd_output_module)
		return snd_output_module->functions->snd_output->pS_O_Shutdown ();
}

int
S_O_GetDMAPos (void)
{
	if (snd_output_module)
		return snd_output_module->functions->snd_output->pS_O_GetDMAPos ();
	else
		return 0; // FIXME: good value for this?
}

void
S_O_Submit (void)
{
	if (snd_output_module)
		snd_output_module->functions->snd_output->pS_O_Submit ();
}

void
S_O_BlockSound (void)
{
	if (snd_output_module)
		snd_output_module->functions->snd_output->pS_O_BlockSound ();
}

void
S_O_UnblockSound (void)
{
	if (snd_output_module)
		snd_output_module->functions->snd_output->pS_O_UnblockSound ();
}
