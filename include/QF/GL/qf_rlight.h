/*
	qf_rlight.h

	GL light stuff from the renderer.

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

#ifndef __QF_GL_rlight_h
#define __QF_GL_rlight_h

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#ifdef HAVE_WINDOWS_H
# include <windows.h>
#endif

extern float bubble_sintable[], bubble_costable[];
void AddLightBlend (float, float, float, float);
void R_RenderDlights (void);

#endif // __QF_GL_rlight_h
