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

*/
static const char rcsid[] = 
	"$Id$";

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
#ifdef HAVE_WINDOWS_H
# include <windows.h>
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

#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/plugin.h"
#include "QF/sys.h"

#include "compat.h"

cvar_t     *fs_pluginpath;

hashtab_t  *registered_plugins;

static const char *
plugin_get_key (void *pl, void *unused)
{
	return ((plugin_list_t *)pl)->name;
}

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
	registered_plugins = Hash_NewTable (253, plugin_get_key, 0, 0);
}

void
PI_Shutdown (void)
{
}

plugin_t *
PI_LoadPlugin (const char *type, const char *name)
{
	char			realname[4096];
	char            plugin_name[1024];
	char            plugin_info_name[1024];
	char			*tmpname;
	void			*dlhand = NULL;
	plugin_t		*plugin = NULL;
	P_PluginInfo	plugin_info = NULL;
	plugin_list_t  *pl;

	if (!name)
		return NULL;

	tmpname = strrchr (name, '/');	// Get the base name, don't allow paths

	// Build the plugin name
	snprintf (plugin_name, sizeof (plugin_name), "%s_%s", type, name);
	pl = Hash_Find (registered_plugins, plugin_name);
	if (pl) {
		plugin_info = pl->info;
	}
	if (!plugin_info) {
		// Build the plugin info name
		snprintf (plugin_info_name, sizeof (plugin_info_name),
				  "%s_%s_PluginInfo", type, name);
		// Build the path to the file to load
#if defined(HAVE_DLOPEN)
		snprintf (realname, sizeof (realname), "%s/lib%s_%s.so",
					fs_pluginpath->string, type,
					(tmpname ? tmpname + 1 : name));
#elif defined(_WIN32)
		snprintf (realname, sizeof (realname), "%s/QF%s_%s.dll",
					fs_pluginpath->string, type,
					(tmpname ? tmpname + 1 : name));
#else
# error "No shared library support. FIXME"
		return NULL;
#endif

#if defined(HAVE_DLOPEN)
		if (!(dlhand = dlopen (realname, RTLD_GLOBAL | RTLD_NOW))) {
			// lib not found
			Sys_Printf ("Could not load plugin \"%s\": %s\n", realname,
						dlerror ());
			return NULL;
		}
#elif defined (_WIN32)
		if (!(dlhand = LoadLibrary (realname))) {	// lib not found
			Sys_Printf ("Could not load plugin \"%s\".\n", realname);
			return NULL;
		}
#endif

#if defined(HAVE_DLOPEN)
		if (!(plugin_info = dlsym (dlhand, plugin_info_name))) {
			// info function not found
			dlclose (dlhand);
			Sys_Printf ("Plugin info function not found\n");
			return NULL;
		}
#elif defined (_WIN32)
		if (!(plugin_info = (P_PluginInfo) GetProcAddress (dlhand,
														   plugin_info_name))) {
			// info function not found
			FreeLibrary (dlhand);
			Sys_Printf ("Plugin info function not found\n");
			return NULL;
		}
#endif
	}

	if (!(plugin = plugin_info ())) {	// Something went badly wrong
#if defined(HAVE_DLOPEN)
		dlclose (dlhand);
#elif defined (_WIN32)
		FreeLibrary (dlhand);
#endif
		Sys_Printf ("Something went badly wrong.\n");
		return NULL;
	}

	plugin->handle = dlhand;
	return plugin;
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
		Sys_DPrintf ("Warning: No shutdown function for type %d plugin!\n", plugin->type);
	}
	if (!plugin->handle) // we didn't load it
		return true;
#if defined(HAVE_DLOPEN)
	return (dlclose (plugin->handle) == 0);
#elif defined (_WIN32)
	return (FreeLibrary (plugin->handle) == 0);
#endif
}

void
PI_RegisterPlugins (plugin_list_t *plugins)
{
	while (plugins->name)
		Hash_Add (registered_plugins, plugins++);
}
