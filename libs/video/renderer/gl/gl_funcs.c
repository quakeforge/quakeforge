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
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include <stdio.h>
#include <dlfcn.h>
#include <QF/GL/types.h>
#include <QF/GL/funcs.h>
#include <QF/cvar.h>
#include <QF/console.h>
#include "r_cvar.h"

// First we need to get all the function pointers declared.
#define QFGL_NEED(ret, name, args)	ret (* QFGL_##name) args = NULL;
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED

// Then we need to open the libGL and set all the symbols.

qboolean GLF_Init ()
{
	void *handle;

	handle = dlopen (gl_libgl->string, RTLD_NOW);
	if (!handle) {
		Con_Printf("Can't open libgl %s: %s\n", gl_libgl->string, dlerror());
		return false;
	}

#define QFGL_NEED(ret, name, args)	\
	QFGL_##name = dlsym(handle, #name); \
	if (!QFGL_##name) { \
		Con_Printf("Can't load symbol %s: %s\n", #name, dlerror()); \
	}
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED

	return true;
}
