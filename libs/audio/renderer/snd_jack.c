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
	"$Id: snd_dma.c 11380 2007-03-10 14:17:52Z taniwha $";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/plugin.h"
#include "QF/sound.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "snd_render.h"

static int snd_blocked = 0;

static void
s_ambient_off (void)
{
}

static void
s_ambient_on (void)
{
}

static void
s_start_sound (int entnum, int entchannel, sfx_t *sfx, const vec3_t origin,
				float fvol, float attenuation)
{
}

static void
s_stop_sound (int entnum, int entchannel)
{
}

static void
s_static_sound (sfx_t *sfx, const vec3_t origin, float vol,
				 float attenuation)
{
}

static void
s_local_sound (const char *sound)
{
}

static void
s_update (const vec3_t origin, const vec3_t forward, const vec3_t right,
			const vec3_t up)
{
}

static void
s_extra_update (void)
{
}

static void
s_block_sound (void)
{
	if (++snd_blocked == 1) {
	}
}

static void
s_unblock_sound (void)
{
	if (!snd_blocked)
		return;

	if (!--snd_blocked) {
	}
}

static void
s_init (void)
{
	SND_SFX_Init ();
}

static void
s_shutdown (void)
{
}

static general_funcs_t plugin_info_general_funcs = {
	s_init,
	s_shutdown,
};

static snd_render_funcs_t plugin_info_render_funcs = {
	s_ambient_off,
	s_ambient_on,
	SND_TouchSound,
	s_static_sound,
	s_start_sound,
	s_stop_sound,
	SND_PrecacheSound,
	s_update,
	SND_StopAllSounds,
	s_extra_update,
	s_local_sound,
	s_block_sound,
	s_unblock_sound,
	SND_LoadSound,
	SND_AllocChannel,
};

static general_data_t plugin_info_general_data;
static snd_render_data_t render_data;

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
	&render_data,
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

PLUGIN_INFO(snd_render, jack)
{
	return &plugin_info;
}
