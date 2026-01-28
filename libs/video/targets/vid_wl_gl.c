/*
	vid_wl_gl.c

	OpenGL Wayland video driver

	Copyright (C) 1996-1997  Id Software, Inc.
	Copyright (C) 1999-2000  contributors of the QuakeForge project
	Copyright (C) 2000       Marcus Sundberg [mackan@stacken.kth.se]
	Copyright (C) 1999,2000  contributors of the QuakeForge project
	Please see the file "AUTHORS" for a list of contributors

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

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <dlfcn.h>
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/sys.h"
#include "QF/vid.h"

#include "context_wl.h"
#include "r_internal.h"
#include "vid_internal.h"
#include "vid_gl.h"

gl_ctx_t *WL_GL_Context (vid_internal_t *vi) __attribute__((const));
gl_ctx_t *
WL_GL_Context (vid_internal_t *vi)
{
	return nullptr;
}

void
WL_GL_Init_Cvars (void)
{
}
