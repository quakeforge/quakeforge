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

	$Id$
*/

#ifndef __QF_GL_vid_h
#define __QF_GL_vid_h

#include "QF/qtypes.h"
#include "QF/GL/types.h"
#include "QF/GL/extensions.h"

// Multitexturing
extern QF_glActiveTextureARB	qglActiveTexture;
extern QF_glMultiTexCoord2fARB	qglMultiTexCoord2f;
extern QF_glMultiTexCoord2fvARB	qglMultiTexCoord2fv;
extern qboolean					gl_mtex_active;
extern qboolean					gl_mtex_capable;
extern GLenum					gl_mtex_enum;
extern float					gldepthmin, gldepthmax;
extern int						texture_extension_number;

extern qboolean					gl_feature_mach64;

void GL_EndRendering (void);
void GL_BeginRendering (int *x, int *y, int *width, int *height);

#endif // __QF_GL_vid_h
