/*
	glx_funcs_list.h

	QF GLX function list.

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

#ifndef QFGL_DONT_NEED
#define QFGL_DONT_NEED(ret, func, params)
#define UNDEF_QFGL_DONT_NEED
#endif

#ifndef QFGL_WANT
#define QFGL_WANT(ret, func, params)
#define UNDEF_QFGL_WANT
#endif

#ifndef QFGL_NEED
#define QFGL_NEED(ret, func, params)
#define UNDEF_QFGL_NEED
#endif

QFGL_NEED (void, glXSwapBuffers, (Display *dpy, GLXDrawable drawable))
QFGL_NEED (void, glXChooseVisual, (Display *dpy, int screen, int *attribList))
QFGL_NEED (void, glXCreateContext, (Display *dpy, XVisualInfo *vis, GLXContext shareList, Bool direct))
QFGL_NEED (void, glXMakeCurrent, (Display *dpy, GLXDrawable drawable, GLXContext ctx))

#ifdef UNDEF_QFGL_DONT_NEED
#undef QFGL_DONT_NEED
#endif

#ifdef UNDEF_QFGL_WANT
#undef QFGL_WANT
#endif

#ifdef UNDEF_QFGL_NEED
#undef QFGL_DONT_NEED
#endif
