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

#ifndef __QF_GLSL_vid_h
#define __QF_GLSL_vid_h

#include "QF/qtypes.h"

typedef struct shaderparam_s {
	const char *name;
	qboolean    uniform;
	int         location;
} shaderparam_t;

extern int glsl_palette;
extern int glsl_colormap;

void GLSL_EndRendering (void);
void GLSL_Init_Common (void);
int GLSL_CompileShader (const char *name, const char *shader_src, int type);
int GLSL_LinkProgram (const char *name, int vert, int frag);
int GLSL_ResolveShaderParam (int program, shaderparam_t *param);
void GLSL_DumpAttribArrays (void);

#endif // __QF_GLSL_vid_h
