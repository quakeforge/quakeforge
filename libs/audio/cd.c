/*
	cd_plugin.c

	cd plugin wrapper

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

#include "QF/cdaudio.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/qargs.h"
#include "QF/qtypes.h"
#include "QF/sys.h"

#include "QF/plugin/general.h"
#include "QF/plugin/cd.h"

cvar_t         *cd_plugin;
plugin_t       *cdmodule = NULL;

CD_PLUGIN_PROTOS
plugin_list_t   cd_plugin_list[] = {
	CD_PLUGIN_LIST
};

VISIBLE void
CDAudio_Pause (void)
{
	if (cdmodule)
		cdmodule->functions->cd->pause ();
}

VISIBLE void
CDAudio_Play (int track, qboolean looping)
{
	if (cdmodule)
		cdmodule->functions->cd->play (track, looping);
}

VISIBLE void
CDAudio_Resume (void)
{
	if (cdmodule)
		cdmodule->functions->cd->resume ();
}

static void
CDAudio_shutdown (void *data)
{
	if (cdmodule)
		cdmodule->functions->general->shutdown ();
}

VISIBLE void
CDAudio_Update (void)
{
	if (cdmodule)
		cdmodule->functions->cd->update ();
}

static void
CD_f (void)
{
	if (cdmodule)
		cdmodule->functions->cd->cd_f ();
}

VISIBLE int
CDAudio_Init (void)
{
	Sys_RegisterShutdown (CDAudio_shutdown, 0);

	PI_RegisterPlugins (cd_plugin_list);
	cd_plugin = Cvar_Get ("cd_plugin", CD_DEFAULT, CVAR_ROM, NULL,
						  "CD Plugin to use");

	if (COM_CheckParm ("-nocdaudio"))
		return 0;

	if (!*cd_plugin->string) {
		Sys_Printf ("Not loading CD due to no driver\n");
		return 0;
	}
	cdmodule = PI_LoadPlugin ("cd", cd_plugin->string);
	if (!cdmodule) {
		Sys_Printf ("Loading of cd module: %s failed!\n", cd_plugin->string);
		return -1;
	}
	cdmodule->functions->general->init ();
	Cmd_AddCommand (
		"cd", CD_f, "Control the CD player.\n"
		"Commands:\n"
		"eject - Eject the CD.\n"
		"info - Reports information on the CD.\n"
		"loop (track number) - Loops the specified track.\n"
		"remap (track1) (track2) ... - Remap the current track order.\n"
		"reset - Causes the CD audio to re-initialize.\n"
		"resume - Will resume playback after pause.\n"
		"off - Shuts down the CD audio system..\n"
		"on - Re-enables the CD audio system after a cd off command.\n"
		"pause - Pause the CD playback.\n"
		"play (track number) - Plays the specified track one time.\n"
		"stop - Stops the currently playing track.");
	Sys_Printf ("CD Audio Initialized\n");
	return 0;
}
