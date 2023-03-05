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

#ifndef __QF_GLSL_vid_h
#define __QF_GLSL_vid_h

#include "QF/qtypes.h"

typedef struct shader_s {
	int         num_strings;
	const char **strings;
	const char **src;
} shader_t;

typedef struct shaderparam_s {
	const char *name;
	qboolean    uniform;
	int         location;
} shaderparam_t;

extern int glsl_palette;
extern int glsl_colormap;

void GLSL_Init_Common (void);
void GLSL_Shutdown_Common (void);

int GLSL_CompileShader (const char *name, const shader_t *shader, int type);
int GLSL_LinkProgram (const char *name, int vert, int frag);
int GLSL_ResolveShaderParam (int program, shaderparam_t *param);
void GLSL_DumpAttribArrays (void);

/*	Register a shader effect "file".

	This is based on The OpenGL Shader Wrangler by the little grasshopper
	(http://prideout.net/blog/?p=11).

	\param name		The name of the effect in the effect key.
	\param src		The string holding the effect file.
	\return			0 for failure, 1 for success.
*/
int GLSL_RegisterEffect (const char *name, const char *src);
void GLSL_ShaderShutdown (void);

/*	Build a shader program script from a list of effect keys.

	This is based on The OpenGL Shader Wrangler by the little grasshopper
	(http://prideout.net/blog/?p=11).

	The returned shader program script is suitable for passing directly to
	GLSL_CompileShader.

	\param effect_keys	Null terminated list of effect keys. The shader will be
					built from the specified segments in the given order.
	\return			A pointer to the built up shader program script, or null on
					failure.
*/
shader_t *GLSL_BuildShader (const char **effect_keys);

/*	Free a shader program script built by GLSL_BuildShader.

	\param shader	The shader program script to be freed.
*/
void GLSL_FreeShader (shader_t *shader);

#endif // __QF_GLSL_vid_h
