/*
	snd_null.c

	include this instead of all the other snd_* files to have no sound
	code whatsoever

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
#include "QF/plugin.h"
#include "QF/sound.h"

static plugin_t				plugin_info;
static plugin_data_t		plugin_info_data;
static plugin_funcs_t		plugin_info_funcs;
static general_data_t		plugin_info_general_data;
static general_funcs_t		plugin_info_general_funcs;
static snd_output_data_t	plugin_info_snd_output_data;
static snd_output_funcs_t	plugin_info_snd_output_funcs;

static void
SNDDMA_Init (void)
{
}

static void
SNDDMA_Shutdown (void)
{
}

static void
SNDDMA_BlockSound (void)
{
}

static int
SNDDMA_GetDMAPos (void)
{
	return 0;
}

static void
SNDDMA_Submit (void)
{
}

static void
SNDDMA_UnblockSound (void)
{
}

QFPLUGIN plugin_t *
PLUGIN_INFO(snd_output, null) (void) {
	plugin_info.type = qfp_snd_output;
	plugin_info.api_version = QFPLUGIN_VERSION;
	plugin_info.plugin_version = "0.1";
	plugin_info.description = "Null sound output driver";
	plugin_info.copyright = "Copyright (C) 1996-1997 id Software, Inc.\n"
		"Copyright (C) 1999,2000,2001  contributors of the QuakeForge project\n"
		"Please see the file \"AUTHORS\" for a list of contributors";
	plugin_info.functions = &plugin_info_funcs;
	plugin_info.data = &plugin_info_data;

	plugin_info_data.general = &plugin_info_general_data;
	plugin_info_data.input = NULL;
	plugin_info_data.snd_output = &plugin_info_snd_output_data;

	plugin_info_funcs.general = &plugin_info_general_funcs;
	plugin_info_funcs.input = NULL;
	plugin_info_funcs.snd_output = &plugin_info_snd_output_funcs;

	plugin_info_general_funcs.p_Init = SNDDMA_Init;
	plugin_info_general_funcs.p_Shutdown = SNDDMA_Shutdown;

	plugin_info_snd_output_funcs.pS_O_BlockSound = SNDDMA_BlockSound;
	plugin_info_snd_output_funcs.pS_O_GetDMAPos = SNDDMA_GetDMAPos;
	plugin_info_snd_output_funcs.pS_O_Submit = SNDDMA_Submit;
	plugin_info_snd_output_funcs.pS_O_UnblockSound = SNDDMA_UnblockSound;

	return &plugin_info;
}
