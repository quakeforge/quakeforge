/*
	gl_textures.h

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

*/

#ifndef __QF_GL_vid_h
#define __QF_GL_vid_h

#include "QF/qtypes.h"
#include "QF/GL/types.h"
#include "QF/GL/extensions.h"

extern byte gl_15to8table[65536];

// Multitexturing
extern QF_glActiveTexture		qglActiveTexture;
extern QF_glMultiTexCoord2f		qglMultiTexCoord2f;
extern QF_glMultiTexCoord2fv	qglMultiTexCoord2fv;
extern int						gl_mtex_active_tmus;
extern bool						gl_mtex_capable;
extern bool						gl_mtex_fullbright;
extern GLenum					gl_mtex_enum;
extern bool						gl_combine_capable;
extern float					gl_rgb_scale;

extern bool						gl_feature_mach64;
extern float					gldepthmin, gldepthmax;
extern int						gl_use_bgra;
extern int						gl_tess;

extern int						gl_max_lights;

void GL_Init_Common (void);

#endif // __QF_GL_vid_h
