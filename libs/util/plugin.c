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

static const char *pi_error = "";

static const char *
plugin_get_key (void *pl, void *unused)
{
	return ((plugin_list_t *) pl)->name;
}

static int
pi_close_lib (void *handle)
{
#if defined(HAVE_DLOPEN)
	return (dlclose (handle) == 0);
#elif defined (_WIN32)
	return (FreeLibrary (handle) == 0);
#endif
}

static void *
pi_get_symbol (void *handle, const char *name)
{
#if defined(HAVE_DLOPEN)
	return dlsym (handle, name);
#elif defined (_WIN32)
	return GetProcAddress (handle, name);
#endif
}

static void *
pi_open_lib (const char *name)
{
	void       *dlhand;

#if defined(HAVE_DLOPEN)
	if (!(dlhand = dlopen (name, RTLD_NOW))) {
		pi_error = dlerror ();
		return 0;
	}
#elif defined (_WIN32)
	if (!(dlhand = LoadLibrary (name))) {	// lib not found
		pi_error = "LoadLibrary failed";
		return 0;
	}
#endif
	pi_error = "";
	return dlhand;
}

static void
pi_realname (char *realname, int size, const char *type, const char *name)
{
#if defined(HAVE_DLOPEN)
	const char *format = "%s/%s_%s.so";
#elif defined(_WIN32)
	const char *format = "%s/QF%s_%s.dll";
#else
# error "No shared library support. FIXME"
#endif

	snprintf (realname, size, format, fs_pluginpath->string, type, name);
}

static void
pi_info_name (char *info_name, int size, const char *type, const char *name)
{
	if (type && name)
		snprintf (info_name, size, "%s_%s_PluginInfo", type, name);
	else if (type)
		snprintf (info_name, size, "%s_PluginInfo", type);
	else
		snprintf (info_name, size, "PluginInfo");
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
	plugin_list_t	*pl;

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
		// Build the path to the file to load
		pi_realname (realname, sizeof (realname), type,
					(tmpname ? tmpname + 1 : name));

		if (!(dlhand = pi_open_lib (realname))) {
			// lib not found
			Sys_Printf ("Could not load plugin \"%s\".\n", realname);
			Sys_DPrintf ("Reason: \"%s\".\n", pi_error);
			return NULL;
		}

		// Build the plugin info name as $type_$name_PluginInfo
		pi_info_name (plugin_info_name, sizeof (plugin_info_name), type, name);
		if (!(plugin_info = pi_get_symbol (dlhand, plugin_info_name))) {
			// Build the plugin info name as $type_PluginInfo
			pi_info_name (plugin_info_name, sizeof (plugin_info_name),
						  type, 0);
			if (!(plugin_info = pi_get_symbol (dlhand, plugin_info_name))) {
				// Build the plugin info name as PluginInfo
				pi_info_name (plugin_info_name, sizeof (plugin_info_name),
							  0, 0);
				if (!(plugin_info = pi_get_symbol (dlhand, plugin_info_name))) {
					// info function not found
					pi_close_lib (dlhand);
					Sys_Printf ("Plugin info function not found\n");
					return NULL;
				}
			}
		}
	}

	if (!(plugin = plugin_info ())) {	// Something went badly wrong
		pi_close_lib (dlhand);
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
		Sys_DPrintf ("Warning: No shutdown function for type %d plugin!\n",
					 plugin->type);
	}
	if (!plugin->handle) // we didn't load it
		return true;
	return pi_close_lib (plugin->handle);
}

void
PI_RegisterPlugins (plugin_list_t *plugins)
{
	while (plugins->name)
		Hash_Add (registered_plugins, plugins++);
}
