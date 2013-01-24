/*
	funcs.h

	GL function defs.

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

#ifndef __QF_GL_funcs_h_
#define __QF_GL_funcs_h_

#include "QF/GL/extensions.h"
#include "QF/GL/types.h"
#include "QF/qtypes.h"

#ifdef _WIN32
# include <windows.h>
#endif

#define QFGL_WANT(ret, name, args)	extern ret (GLAPIENTRY * qf##name) args;
#define QFGL_NEED(ret, name, args)	extern ret (GLAPIENTRY * qf##name) args;
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED
#undef QFGL_WANT

qboolean GLF_FindFunctions (void);

#endif // __QF_GL_funcs_h_
