/*
	gl_types.h

	GL texture stuff from the renderer.

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

#ifndef __gl_types_h
#define __gl_types_h

#include "QF/qtypes.h"

typedef unsigned int	GLenum;
typedef unsigned char	GLboolean;
typedef unsigned int	GLbitfield;
typedef void			GLvoid;
typedef signed char		GLbyte;			/* 1-byte signed */
typedef short			GLshort;		/* 2-byte signed */
typedef int				GLint;			/* 4-byte signed */
typedef unsigned char	GLubyte;		/* 1-byte unsigned */
typedef unsigned short	GLushort;		/* 2-byte unsigned */
typedef unsigned int	GLuint;			/* 4-byte unsigned */
typedef int				GLsizei;		/* 4-byte signed */
typedef float			GLfloat;		/* single precision float */
typedef float			GLclampf;		/* single precision float in [0,1] */
typedef double			GLdouble;		/* double precision float */
typedef double			GLclampd;		/* double precision float in [0,1] */

#endif // __gl_types_h
