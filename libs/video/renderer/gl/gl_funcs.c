/*
	gl_funcs.c

	GL functions.

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

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdio.h>

#include <QF/GL/types.h>
#include <QF/GL/funcs.h>
#include <QF/GL/extensions.h>
#include <QF/cvar.h>
#include <QF/console.h>
#include "r_cvar.h"

// First we need to get all the function pointers declared.
#define QFGL_NEED(ret, name, args) \
typedef ret (* QF_##name) args; \
QF_##name		name = NULL;

#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED

// Then we need to open the libGL and set all the symbols.
qboolean
GLF_Init (void)
{
	void	*handle;

#ifdef HAVE_DLOPEN
	if (!(handle = dlopen (gl_libgl->string, RTLD_NOW))) {
		Con_Printf ("Couldn't load OpenGL library %s: %s\n", gl_libgl->string, dlerror ());
#else
# if _WIN32
	if (!(handle = LoadLibrary (gl_libgl->string))) {
		Con_Printf ("Couldn't load OpenGL library %s!\n", gl_libgl->string);
# else
	{
		Con_Printf ("Cannot load libraries: %s was built without DSO support", PROGRAM);
# endif
#endif
		return false;
	}

#define QFGL_NEED(ret, name, args)	\
	name = QFGL_ProcAddress (handle, (#name));
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED

	return true;
}

void *
QFGL_ProcAddress (void *handle, const char *name)
{
#if defined(HAVE_GLX) && defined(HAVE_DLOPEN)
	static QF_glXGetProcAddressARB	glXGetProcAddress = NULL;
	static qboolean 				inited = false;

	if (!inited) {
		inited = true;
		glXGetProcAddress = dlsym (handle, "glXGetProcAddressARB");
	}
#endif

	if (name) {
#if defined(HAVE_GLX) && defined(HAVE_DLOPEN)
		if (glXGetProcAddress) {
			return glXGetProcAddress ((const GLubyte *) name);
		} else {
			void	*glfunction;

			if (!(glfunction = dlsym (handle, name))) {
				Con_Printf ("Cannot find symbol %s: %s\n", name, dlerror ());
				return NULL;
			}

			return glfunction;
		}
	}
#else
# ifdef _WIN32
	void	*glfunction;
	char	filename[4096];

	if (!(glfunction = GetProcAddress (handle, name))) {
		if (GetModuleFileName (handle, &filename, sizeof (filename)))
			Con_Printf ("Cannot find symbol %s in library %s", name, filename);
		else
			Con_Printf ("Cannot find symbol %s in library %s", name, gl_libgl->string);
		return NULL;
	}
	
	return glfunction;
# endif
#endif
	return NULL;
}
