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
# include "winquake.h"
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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/hash.h"
#include "QF/plugin.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/plugin/general.h"

#include "compat.h"

// loaded_plugins is only for plugins loaded from scripts - not system
// plugins

typedef struct loaded_plugin_s {
	char			*name;
	plugin_t		*plugin;
} loaded_plugin_t;

cvar_t     *fs_pluginpath;

hashtab_t  *registered_plugins, *loaded_plugins;

static const char *pi_error = "";

static const char *
plugin_get_key (const void *pl, void *unused)
{
	return ((plugin_list_t *) pl)->name;
}

static const char *
loaded_plugin_get_key (const void *lp, void *unusued)
{
	return ((loaded_plugin_t *) lp)->name;
}

static void
loaded_plugin_delete (void *lp, void *unused)
{
	free (((loaded_plugin_t *) lp)->name);
	free ((loaded_plugin_t *) lp);
}

static int
pi_close_lib (void *handle)
{
	if (handle) {
#if defined(HAVE_DLOPEN)
		return (dlclose (handle) == 0);
#elif defined (_WIN32)
		return (FreeLibrary (handle) == 0);
#endif
	}
	return 1;
}

static void *
pi_get_symbol (void *handle, const char *name)
{
#if defined(HAVE_DLOPEN)
	return dlsym (handle, name);
#elif defined (_WIN32)
	return GetProcAddress (handle, name);
#endif
	return 0;
}

static void *
pi_open_lib (const char *name, int global_syms)
{
	void       *dlhand = 0;
#ifdef HAVE_DLOPEN
# ifdef __OpenBSD__
	int        flags = RTLD_LAZY;
# else
	int        flags = RTLD_NOW;
# endif
#endif

#if defined(HAVE_DLOPEN)
# if defined(RTLD_GLOBAL)
	if (global_syms)
		flags |= RTLD_GLOBAL;
# endif
	if (!(dlhand = dlopen (name, flags))) {
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

static const char *
pi_realname (const char *type, const char *name)
{
#if defined(HAVE_DLOPEN)
	return va (0, "%s/%s_%s.so", fs_pluginpath->string, type, name);
#elif defined(_WIN32)
	return va (0, "%s/%s_%s.dll", fs_pluginpath->string, type, name);
#else
	return "No shared library support. FIXME";
#endif
}

static const char *
pi_info_name (const char *type, const char *name)
{
	if (type && name) {
		return va (0, "%s_%s_PluginInfo", type, name);
	} else if (type) {
		return va (0, "%s_PluginInfo", type);
	} else {
		return "PluginInfo";
	}
}

static void
PI_InitCvars (void)
{
	fs_pluginpath = Cvar_Get ("fs_pluginpath", FS_PLUGINPATH, CVAR_ROM, NULL,
							"Location of your plugin directory");
}

static void
PI_Plugin_Load_f (void)
{
	plugin_t *pi;
	const char *type, *name;

	if (Cmd_Argc() != 3) {
		Sys_Printf ("Usage: plugin_load <type> <name>\n");
		return;
	}

	type = Cmd_Argv(1);
	name = Cmd_Argv(2);

	pi = PI_LoadPlugin (type, name);
	if (!pi) {
		Sys_Printf ("Error loading plugin %s %s\n", type, name);
	}
}

static void
PI_Plugin_Unload_f (void)
{
	const char *plugin_name;
	loaded_plugin_t *lp;
	plugin_t *pi;

	if (Cmd_Argc() != 3) {
		Sys_Printf ("Usage: plugin_unload <type> <name>\n");
		return;
	}

	// try to locate the plugin
	plugin_name = va (0, "%s_%s", Cmd_Argv(1), Cmd_Argv(2));

	lp = Hash_Find (loaded_plugins, plugin_name);
	if (lp) {
		pi = lp->plugin;
	} else {
		Sys_Printf ("Plugin %s not loaded\n", plugin_name);
		return;
	}

	PI_UnloadPlugin (pi);
}

/*
	PI_Init

	Eventually this function will likely initialize libltdl. It doesn't, yet,
	since we aren't using libltdl yet.
*/
VISIBLE void
PI_Init (void)
{
	PI_InitCvars ();
	registered_plugins = Hash_NewTable (253, plugin_get_key, 0, 0, 0);
	loaded_plugins = Hash_NewTable(253, loaded_plugin_get_key,
								   loaded_plugin_delete, 0, 0);
	Cmd_AddCommand("plugin_load", PI_Plugin_Load_f,
				   "load the plugin of the given type name and name");
	Cmd_AddCommand("plugin_unload", PI_Plugin_Unload_f,
				   "unload the plugin of the given type name and name");
}

void
PI_Shutdown (void)
{
	void **elems, **cur;

	// unload all "loaded" plugins and free the hash
	elems = Hash_GetList (loaded_plugins);
	for (cur = elems; *cur; ++cur)
		PI_UnloadPlugin (((loaded_plugin_t *) *cur)->plugin);
	free (elems);

	Hash_DelTable (loaded_plugins);

}

VISIBLE plugin_t *
PI_LoadPlugin (const char *type, const char *name)
{
	const char *realname = 0;
	const char *plugin_name = 0;
	const char *plugin_info_name = 0;
	const char *tmpname;
	void       *dlhand = 0;
	plugin_t   *plugin = 0;
	plugin_info_t plugin_info = 0;
	plugin_list_t *pl;
	loaded_plugin_t *lp;

	if (!name)
		return NULL;

	// Get the base name, don't allow paths
	tmpname = strrchr (name, '/');
	if (tmpname) {
		name = tmpname + 1;	// skip over '/'
	}

	// Build the plugin name
	plugin_name = va (0, "%s_%s", type, name);

	// make sure we're not already loaded
	lp = Hash_Find (loaded_plugins, plugin_name);
	if (lp) {
		Sys_Printf ("Plugin %s already loaded\n", plugin_name);
		return NULL;
	}

	pl = Hash_Find (registered_plugins, plugin_name);
	if (pl) {
		plugin_info = pl->info;
	}
	if (!plugin_info) {
		// Build the path to the file to load
		realname = pi_realname (type, name);

		if (!(dlhand = pi_open_lib (realname, 0))) {
			// lib not found
			Sys_Printf ("Could not load plugin \"%s\".\n", realname);
			Sys_Printf ("Reason: \"%s\".\n", pi_error);
			return NULL;
		}

		// Build the plugin info name as $type_$name_PluginInfo
		plugin_info_name = pi_info_name (type, name);
		if (!(plugin_info = pi_get_symbol (dlhand, plugin_info_name))) {
			// Build the plugin info name as $type_PluginInfo
			plugin_info_name = pi_info_name (type, 0);
			if (!(plugin_info = pi_get_symbol (dlhand, plugin_info_name))) {
				// Build the plugin info name as PluginInfo
				plugin_info_name = pi_info_name (0, 0);
				if (!(plugin_info = pi_get_symbol (dlhand, plugin_info_name))) {
					// info function not found
					pi_close_lib (dlhand);
					Sys_Printf ("Plugin info function not found\n");
					return NULL;
				}
			}
		}

		// get the plugin data structure
		if (!(plugin = plugin_info ())) {
			pi_close_lib (dlhand);
			Sys_Printf ("Something went badly wrong.\n");
			return NULL;
		}

#if defined(HAVE_DLOPEN) && defined(RTLD_GLOBAL)
		// check if it wants its syms to be global
		if (plugin->data->general->flag & PIF_GLOBAL) {
			// do the whole thing over again with global syms
			pi_close_lib (dlhand);

			// try to reopen
			if (!(dlhand = pi_open_lib (realname, 1))) {
				Sys_Printf ("Error reopening plugin \"%s\".\n", realname);
				Sys_MaskPrintf (SYS_dev, "Reason: \"%s\".\n", pi_error);
				return NULL;
			}

			// get the plugin_info func pointer
			if (!(plugin_info = pi_get_symbol (dlhand, plugin_info_name))) {
				pi_close_lib (dlhand);
				Sys_Printf ("Plugin info function missing on reload\n");
				return NULL;
			}

			// get the plugin data structure
			if (!(plugin = plugin_info ())) {
				pi_close_lib (dlhand);
				Sys_Printf ("Something went badly wrong on module reload\n");
				return NULL;
			}
		}
#endif

	} else if (!(plugin = plugin_info ())) {	// Something went badly wrong
		pi_close_lib (dlhand);
		Sys_Printf ("Something went badly wrong.\n");
		return NULL;
	}

	// add the plugin to the loaded_plugins hashtable
	lp = malloc (sizeof (loaded_plugin_t));
	lp->name = strdup (plugin_name);
	lp->plugin = plugin;
	Hash_Add (loaded_plugins, lp);

	plugin->full_name = lp->name;
	plugin->handle = dlhand;

	if (plugin->functions && plugin->functions->general
		&& plugin->functions->general->init) {
		plugin->functions->general->init ();
	}
	return plugin;
}

VISIBLE qboolean
PI_UnloadPlugin (plugin_t *plugin)
{
	if (plugin && plugin->functions && plugin->functions->general
		&& plugin->functions->general->shutdown) {
		plugin->functions->general->shutdown ();
	} else {
		Sys_MaskPrintf (SYS_dev,
						"Warning: No shutdown function for type %d plugin!\n",
						plugin->type);
	}

	// remove from the table of loaded plugins
	Hash_Free (loaded_plugins, Hash_Del (loaded_plugins, plugin->full_name));

	if (!plugin->handle) // we didn't load it
		return true;
	return pi_close_lib (plugin->handle);
}

VISIBLE void
PI_RegisterPlugins (plugin_list_t *plugins)
{
	while (plugins->name)
		Hash_Add (registered_plugins, plugins++);
}
