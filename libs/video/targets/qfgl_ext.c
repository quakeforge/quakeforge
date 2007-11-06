/*
	qfgl_ext.c

	QuakeForge OpenGL extension interface definitions

	Copyright (C) 2000 Jeff Teunissen <deek@dusknet.dhs.org>

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

static __attribute__ ((used)) const char rcsid[] = 
	"$Id$";

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

#ifdef _WIN32
// must be BEFORE include gl/gl.h
# include "winquake.h"
#endif
#ifdef HAVE_WINDOWS_H
# include <windows.h>
#endif

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/cvar.h"
#include "QF/qtypes.h"
#include "QF/sys.h"
#include "QF/GL/extensions.h"
#include "QF/GL/funcs.h"

#include "r_cvar.h"


void *
QFGL_ProcAddress (void *handle, const char *name, qboolean crit)
{
	void	*glfunc = NULL;

	Sys_DPrintf ("DEBUG: Finding symbol %s ... ", name);

	glfunc = QFGL_GetProcAddress (handle, name);
	if (glfunc) {
		Sys_DPrintf ("found [%p]\n", glfunc);
		return glfunc;
	}
	Sys_DPrintf ("not found\n");

	if (crit) {
		if (strncmp ("fxMesa", name, 6) == 0) {
			Sys_Printf ("This target requires a special version of Mesa with "
						"support for Glide and SVGAlib.\n");
			Sys_Printf ("If you are in X, try using a GLX or SGL target.\n");
		}
		Sys_Error ("Couldn't load critical OpenGL function %s, exiting...",
				   name);
	}
	return NULL;
}

// First we need to get all the function pointers declared.
#define QFGL_WANT(ret, name, args) \
	VISIBLE ret (GLAPIENTRY * qf##name) args;
#define QFGL_NEED(ret, name, args) \
	VISIBLE ret (GLAPIENTRY * qf##name) args;
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED
#undef QFGL_WANT
void		*libgl_handle;

// Then we need to open the libGL and set all the symbols.
qboolean
GLF_Init (void)
{
	libgl_handle = QFGL_LoadLibrary ();
	return true;
}

qboolean
GLF_FindFunctions (void)
{
#define QFGL_WANT(ret, name, args) \
	qf##name = QFGL_ProcAddress (libgl_handle, #name, false);
#define QFGL_NEED(ret, name, args) \
	qf##name = QFGL_ProcAddress (libgl_handle, #name, true);
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED
#undef QFGL_WANT

	return true;
}

/*
  ParseExtensionList

  It takes a bit of care to be fool-proof about parsing an OpenGL extensions
  string. Don't be fooled by sub-strings, etc.
*/
static qboolean
QFGL_ParseExtensionList (const GLubyte *list, const char *name)
{
	const char *start;
	char       *where, *terminator;

	if (!list)
		return 0;

	// Extension names must not have spaces.
	where = strchr (name, ' ');
	if (where || *name == '\0')
		return 0;

	start = (const char *) list;
	for (;;) {
		where = strstr (start, name);
		if (!where)
			break;
		terminator = where + strlen (name);
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return 1;
		start = terminator;
	}
	return 0;
}

VISIBLE qboolean
QFGL_ExtensionPresent (const char *name)
{
	static const GLubyte *gl_extensions = NULL;

	if (!gl_extensions) {				// get and save GL extension list
		gl_extensions = qfglGetString (GL_EXTENSIONS);
	}

	return QFGL_ParseExtensionList (gl_extensions, name);
}

void *
QFGL_ExtensionAddress (const char *name)
{
	static void *handle;

	if (!handle) {
#if defined(HAVE_DLOPEN)
		if (!(handle = dlopen (gl_driver->string, RTLD_NOW))) {
			Sys_Error ("Couldn't load OpenGL library %s: %s",
					   gl_driver->string, dlerror ());
			return 0;
		}
#elif defined(_WIN32)
		if (!(handle = LoadLibrary (gl_driver->string))) {
			Sys_Error ("Couldn't load OpenGL library %s!",
					   gl_driver->string);
			return 0;
		}
#else
# error "Cannot load libraries: %s was not configured with DSO support"
#endif
	}

	if (name)
		return QFGL_ProcAddress (handle, name, false);
	return NULL;
}
