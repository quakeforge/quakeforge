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
#include <QF/sys.h>
#include "r_cvar.h"

// First we need to get all the function pointers declared.
#define QFGL_NEED(ret, name, args) \
ret (* qf##name) args = NULL;
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED

// Then we need to open the libGL and set all the symbols.
qboolean
GLF_Init (void)
{
	void	*handle;

#if defined(HAVE_DLOPEN)
	if (!(handle = dlopen (gl_libgl->string, RTLD_NOW))) {
		Sys_Error ("Couldn't load OpenGL library %s: %s\n", gl_libgl->string, dlerror ());
		return false;
	}
#elif defined(_WIN32)
	if (!(handle = LoadLibrary (gl_libgl->string))) {
		Sys_Error ("Couldn't load OpenGL library %s!\n", gl_libgl->string);
		return false;
	}
#else
# error "Cannot load libraries: %s was not configured with DSO support"
#endif

#define QFGL_NEED(ret, name, args)	\
	qf##name = QFGL_ProcAddress (handle, #name, true);

#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED
	QFGL_ProcAddress (NULL, NULL, false); // tell ProcAddress to clear its cache

	return true;
}

void *
QFGL_ProcAddress (void *handle, const char *name, qboolean crit)
{
	static qboolean inited = false;
	void			*glfunc = NULL;

#if defined(HAVE_DLOPEN)
	static QF_glXGetProcAddressARB	glGetProcAddress = NULL;
#elif defined(_WIN32)
	static void * (* glGetProcAddress) (const char *) = NULL;
#else
	static void *glGetProcAddress = NULL;
#endif

	if (!handle || !name) {
		inited = false;
		return NULL;
	}

	if (!inited) {
		inited = true;
#if defined(HAVE_DLOPEN)
		glGetProcAddress = dlsym (handle, "glXGetProcAddressARB");
#elif defined(_WIN32)
		(FARPROC)glGetProcAddress = GetProcAddress (handle, "wglGetProcAddress");
#endif
	}

	Con_DPrintf ("DEBUG: Finding symbol %s ... ", name);
#if defined(HAVE_DLOPEN)
		glfunc = dlsym (handle, name);
#elif defined(_WIN32)
		glfunc = GetProcAddress (handle, name);
#endif
	if (glGetProcAddress && glfunc != glGetProcAddress (name)) {
		Con_DPrintf ("mismatch! [%p != %p]\n", glfunc, glGetProcAddress (name));
		return glfunc;
	}

	if (!glfunc && glGetProcAddress)
		glfunc = glGetProcAddress (name);

	if (glfunc) {
		Con_DPrintf ("found [%p]\n", glfunc);
		return glfunc;
	}

	Con_DPrintf ("not found\n");

	if (crit)
		Sys_Error ("Couldn't load critical OpenGL function %s, exiting...\n", name);

	return NULL;
}
