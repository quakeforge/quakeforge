/*
	glsl_fisheye.c

	GLSL screen fisheye

	Copyright (C) 2022 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2022/3/25

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
#include <stdlib.h>

#include "QF/cvar.h"
#include "QF/render.h"
#include "QF/sys.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_vid.h"
#include "QF/GLSL/qf_fisheye.h"

#include "r_internal.h"
#include "vid_gl.h"

static const char *fisheye_vert_effects[] =
{
	"QuakeForge.Vertex.fstri",
	0
};

static const char *fisheye_frag_effects[] =
{
	"QuakeForge.Math.const",
	"QuakeForge.Fragment.screen.fisheye",
	0
};

static struct {
	int         program;
	GLuint      vao;
	shaderparam_t screenTex;
	shaderparam_t fov;
	shaderparam_t aspect;
} fisheye = {
	.screenTex = {"screenTex", 1},
	.fov = {"fov", 1},
	.aspect = {"aspect", 1},
};

void
glsl_InitFisheye (void)
{
	shader_t   *vert_shader, *frag_shader;
	int         vert;
	int         frag;

	vert_shader = GLSL_BuildShader (fisheye_vert_effects);
	frag_shader = GLSL_BuildShader (fisheye_frag_effects);
	vert = GLSL_CompileShader ("scrfisheye.vert", vert_shader,
							   GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("scrfisheye.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	fisheye.program = GLSL_LinkProgram ("scrfisheye", vert, frag);
	GLSL_ResolveShaderParam (fisheye.program, &fisheye.screenTex);
	GLSL_ResolveShaderParam (fisheye.program, &fisheye.fov);
	GLSL_ResolveShaderParam (fisheye.program, &fisheye.aspect);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);

	qfeglCreateVertexArrays (1, &fisheye.vao);
}

void
glsl_FisheyeScreen (framebuffer_t *fb)
{
	qfeglUseProgram (fisheye.program);

	qfeglUniform1f (fisheye.fov.location, scr_ffov->value * M_PI / 360);
	qfeglUniform1f (fisheye.aspect.location,
					(float) r_refdef.vrect.height / r_refdef.vrect.width);

	gl_framebuffer_t *buffer = fb->buffer;
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglBindTexture (GL_TEXTURE_CUBE_MAP, buffer->color);

	qfeglBindVertexArray (fisheye.vao);
	qfeglDrawArrays (GL_TRIANGLES, 0, 3);
	qfeglBindVertexArray (0);

	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
}
