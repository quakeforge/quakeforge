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

*/
static const char rcsid[] = 
	"$Id$";

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_WINDOWS_H
# include <windows.h>
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

#include <QF/cvar.h>
#include <QF/console.h>
#include <QF/sys.h>
#include <QF/GL/funcs.h>

#include "r_cvar.h"

// First we need to get all the function pointers declared.
#define QFGL_NEED(ret, name, args) \
ret (GLAPIENTRY * qf##name) args;
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED

void       *libgl_handle;

#if defined(HAVE_DLOPEN)

static QF_glXGetProcAddressARB	glGetProcAddress = NULL;
static void * (*getProcAddress) (void *handle, const char *symbol);

void *
QFGL_LoadLibrary (void)
{
	void	*handle;

	if (!(handle = dlopen (gl_driver->string, RTLD_NOW))) {
		Sys_Error ("Couldn't load OpenGL library %s: %s\n", gl_driver->string,
				   dlerror ());
	}
	getProcAddress = dlsym;
	glGetProcAddress = dlsym (handle, "glXGetProcAddressARB");
	return handle;
}

#elif defined(_WIN32)

static void * (APIENTRY *glGetProcAddress) (const char *) = NULL;
static FARPROC (WINAPI *getProcAddress) (HINSTANCE,LPCSTR);

void *
QFGL_LoadLibrary (void)
{
	void	*handle;

	if (!(handle = LoadLibrary (gl_driver->string))) {
		Sys_Error ("Couldn't load OpenGL library %s!\n", gl_driver->string);
	}
	getProcAddress = GetProcAddress;
	(FARPROC)glGetProcAddress = GetProcAddress (handle, "wglGetProcAddress");
	return handle;
}

#else

# error "Cannot load libraries: %s was not configured with DSO support"

// the following is to avoid other compiler errors
static QF_glXGetProcAddressARB	glGetProcAddress = NULL;
static void * (*getProcAddress) (void *handle, const char *symbol);

void *
QFGL_LoadLibrary (void)
{
	return 0;
}

#endif

// Then we need to open the libGL and set all the symbols.
qboolean
GLF_Init (void)
{
	libgl_handle = QFGL_LoadLibrary ();

#define QFGL_NEED(ret, name, args)	\
	qf##name = QFGL_ProcAddress (libgl_handle, #name, true);
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED

	return true;
}

void *
QFGL_ProcAddress (void *handle, const char *name, qboolean crit)
{
	void			*glfunc = NULL;

	Con_DPrintf ("DEBUG: Finding symbol %s ... ", name);
	if (glGetProcAddress)
		glfunc = glGetProcAddress (name);
	if (!glfunc)
		glfunc = getProcAddress (handle, name);

	if (glfunc) {
		Con_DPrintf ("found [%p]\n", glfunc);
		return glfunc;
	}

	Con_DPrintf ("not found\n");

	if (crit)
		Sys_Error ("Couldn't load critical OpenGL function %s, exiting...\n",
				   name);

	return NULL;
}
