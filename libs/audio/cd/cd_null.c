/*
	cd_null.c

	support for no cd audio

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
		Boston, MA  02111-1307, USA

	$Id$
*/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/cdaudio.h"
#include "QF/plugin.h"

plugin_t		plugin_info;
plugin_data_t	plugin_info_data;
plugin_funcs_t	plugin_info_funcs;
general_data_t	plugin_info_general_data;
general_funcs_t	plugin_info_general_funcs;
//cd_data_t		plugin_info_cd_data;
cd_funcs_t		plugin_info_cd_funcs;


void
I_CDAudio_Pause (void)
{
}


void
I_CDAudio_Play (byte track, qboolean looping)
{
}


void
I_CDAudio_Resume (void)
{
}


void
I_CDAudio_Shutdown (void)
{
}


void
I_CDAudio_Update (void)
{
}


void
I_CDAudio_Init (void)
{
}


void
I_CD_f (void)
{
}


plugin_t *
PluginInfo (void)
{
	plugin_info.type = qfp_cd;
	plugin_info.api_version = QFPLUGIN_VERSION;
	plugin_info.plugin_version = "0.1";
	plugin_info.description = "Null CD Audio output"
		"Copyright (C) 2001  contributors of the QuakeForge project\n"
		"Please see the file \"AUTHORS\" for a list of contributors\n";
	plugin_info.functions = &plugin_info_funcs;
	plugin_info.data = &plugin_info_data;

	plugin_info_data.general = &plugin_info_general_data;
//	plugin_info_data.cd = &plugin_info_cd_data;
	plugin_info_data.input = NULL;

	plugin_info_funcs.general = &plugin_info_general_funcs;
	plugin_info_funcs.cd = &plugin_info_cd_funcs;
	plugin_info_funcs.input = NULL;

	plugin_info_general_funcs.p_Init = I_CDAudio_Init;
	plugin_info_general_funcs.p_Shutdown = I_CDAudio_Shutdown;

	plugin_info_cd_funcs.pCDAudio_Pause = I_CDAudio_Pause;
	plugin_info_cd_funcs.pCDAudio_Play = I_CDAudio_Play;
	plugin_info_cd_funcs.pCDAudio_Resume = I_CDAudio_Resume;
	plugin_info_cd_funcs.pCDAudio_Update = I_CDAudio_Update;
	plugin_info_cd_funcs.pCD_f = I_CD_f;
     
	return &plugin_info;
}
