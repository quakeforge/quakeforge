/*
	plugin.c

	QuakeForge plugin API functions

	Copyright (C) 2001 Jeff Teunissen <deek@quakeforge.net>

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

#ifdef HAVE_STDIO_H
# include <stdio.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#ifdef HAVE_DLFCN_H
# include <dlfcn.h>
#endif
#ifndef RTLD_LAZY
# ifdef DL_LAZY
#  define RTLD_LAZY     DL_LAZY
# else
#  define RTLD_LAZY     0
# endif
#endif

#include <QF/plugin.h>
#include <QF/console.h>

cvar_t	*fs_pluginpath;

void
PI_InitCvars (void)
{
	fs_pluginpath = Cvar_Get ("fs_pluginpath", FS_PLUGINPATH, CVAR_ROM, NULL,
							"Location of your plugin directory");
}

/*
	PI_Init

	Eventually this function will likely initialize libltdl. It doesn't, yet,
	since we aren't using libltdl yet.
*/
void
PI_Init (void)
{
	PI_InitCvars ();
}

void
PI_Shutdown (void)
{
}

plugin_t *
PI_LoadPlugin (char *type, char *name)
{
#ifdef HAVE_DLOPEN
	char			realname[4096];
	char			*tmpname;
	void			*dlhand = NULL;
	plugin_t		*plugin = NULL;
	P_PluginInfo	plugin_info = NULL;

	if (!name)
		return NULL;

	tmpname = strrchr (name, '/');	// Get the base name, don't allow paths

	// Build the path to the file to load
	snprintf (realname, sizeof (realname), "%s/lib%s_%s.so",
				fs_pluginpath->string, type, (tmpname ? tmpname + 1 : name));

	if (!(dlhand = dlopen (realname, RTLD_LAZY))) {	// lib not found
		Con_Printf ("%s\n", dlerror());
		return NULL;
	}

	if (!(plugin_info = dlsym (dlhand, "PluginInfo"))) {	// info function not found
		dlclose (dlhand);
		Con_Printf ("info function not found\n");
		return NULL;
	}

	if (!(plugin = plugin_info ())) {	// Something went badly wrong
		dlclose (dlhand);
		Con_Printf ("soemthing went badly wrong\n");
		return NULL;
	}

	plugin->handle = dlhand;
	return plugin;
#else
	return NULL;	// no plugin support
#endif
}

qboolean
PI_UnloadPlugin (plugin_t *plugin)
{
	if (plugin
			&& plugin->functions
			&& plugin->functions->general
			&& plugin->functions->general->p_Shutdown) {
		plugin->functions->general->p_Shutdown ();
	} else {
		Con_Printf ("Warning: No shutdown function for plugin!");
	}
#ifdef HAVE_DLOPEN
	return (dlclose (plugin->handle) == 0);
#else
	return false;
#endif
}
