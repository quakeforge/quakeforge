/*
	glsl_alias.c

	GLSL Alias model rendering

	Copyright (C) 2012 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2012/1/1

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

static __attribute__ ((used)) const char rcsid[] = "$Id$";

#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_alias.h"
#include "QF/GLSL/qf_vid.h"

static const char quakemdl_vert[] =
#include "quakemdl.vc"
;

static const char quakemdl_frag[] =
#include "quakemdl.fc"
;

static struct {
	int         program;
	shaderparam_t normals;
	shaderparam_t mvp_matrix;
	shaderparam_t norm_matrix;
	shaderparam_t color;
	shaderparam_t stn;
	shaderparam_t vertex;
	shaderparam_t palette;
	shaderparam_t colormap;
	shaderparam_t skin;
	shaderparam_t ambient;
	shaderparam_t shadelight;
	shaderparam_t lightvec;
} quake_mdl = {
	0,
	{"normals", 1},
	{"mvp_mat", 1},
	{"norm_mat", 1},
	{"vcolor", 0},
	{"stn", 0},
	{"vertex", 0},
	{"palette", 1},
	{"colormap", 1},
	{"skin", 1},
	{"ambient", 1},
	{"shadelight", 1},
	{"lightvec", 1},
};

VISIBLE void
R_InitAlias (void)
{
	int         vert;
	int         frag;
	vert = GL_CompileShader ("quakemdl.vert", quakemdl_vert, GL_VERTEX_SHADER);
	frag = GL_CompileShader ("quakemdl.frag", quakemdl_frag,
							 GL_FRAGMENT_SHADER);
	quake_mdl.program = GL_LinkProgram ("quakemdl", vert, frag);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.normals);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.mvp_matrix);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.norm_matrix);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.color);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.stn);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.vertex);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.palette);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.colormap);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.skin);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.ambient);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.shadelight);
	GL_ResolveShaderParam (quake_mdl.program, &quake_mdl.lightvec);
}
