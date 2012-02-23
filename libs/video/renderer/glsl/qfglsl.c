/*
	qfglsl.c

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
#include "QF/GLSL/funcs.h"

#include "r_internal.h"


void *
QFEGL_ProcAddress (void *handle, const char *name, qboolean crit)
{
	void	*glfunc = NULL;

	Sys_MaskPrintf (SYS_VID, "DEBUG: Finding symbol %s ... ", name);

	glfunc = vid.get_proc_address (handle, name);
	if (glfunc) {
		Sys_MaskPrintf (SYS_VID, "found [%p]\n", glfunc);
		return glfunc;
	}
	Sys_MaskPrintf (SYS_VID, "not found\n");

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
	ret (GLAPIENTRY * qfe##name) args;
#define QFGL_NEED(ret, name, args) \
	ret (GLAPIENTRY * qfe##name) args;
#include "QF/GLSL/qf_funcs_list.h"
#undef QFGL_NEED
#undef QFGL_WANT
static void		*libgl_handle;

// Then we need to open the libGL and set all the symbols.
qboolean
EGLF_Init (void)
{
	libgl_handle = vid.load_library ();
	return true;
}

qboolean
EGLF_FindFunctions (void)
{
#define QFGL_WANT(ret, name, args) \
	qfe##name = QFEGL_ProcAddress (libgl_handle, #name, false);
#define QFGL_NEED(ret, name, args) \
	qfe##name = QFEGL_ProcAddress (libgl_handle, #name, true);
#include "QF/GLSL/qf_funcs_list.h"
#undef QFGL_NEED
#undef QFGL_WANT

	return true;
}
