/*
	glsl_trails.c

	OpenGL trail system.

	Copyright (C) 2013  	Bill Currie <bill@taniwha.org>

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

#include "QF/alloc.h"
#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/image.h"
#include "QF/mersenne.h"
#include "QF/qargs.h"
#include "QF/quakefs.h"
#include "QF/render.h"
#include "QF/sys.h"
#include "QF/va.h"

#include "QF/scene/entity.h"

#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
#include "QF/GLSL/qf_particles.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_internal.h"
#include "r_local.h"

static uint32_t     maxparticles;
static GLushort    *pVAindices;
static partvert_t  *particleVertexArray;

static const char *particle_trail_vert_effects[] =
{
	"QuakeForge.Screen.viewport",
	"QuakeForge.Vertex.transform.view_projection",
	"QuakeForge.Vertex.ScreenSpace.curve.width",
	"QuakeForge.Vertex.particle.trail",
	0
};

static const char *particle_trail_frag_effects[] =
{
	"QuakeForge.Math.InvSqrt",
	"QuakeForge.Math.permute",
	"QuakeForge.Noise.simplex",
	"QuakeForge.Fragment.particle.trail",
	0
};

static const char *particle_trail_debug_frag_effects[] =
{
	"QuakeForge.Fragment.barycentric",
	0
};

typedef struct {
	int         program;
	shaderparam_t proj;
	shaderparam_t view;
	shaderparam_t viewport;
	shaderparam_t width;
	shaderparam_t last;
	shaderparam_t current;
	shaderparam_t next;
	shaderparam_t barycentric;
	shaderparam_t texoff;
	shaderparam_t colora;
	shaderparam_t colorb;
} trailprog_t;

static trailprog_t trail = {
	0,
	{"projection_mat", 1},
	{"view_mat", 1},
	{"viewport", 1},
	{"width", 1},
	{"last", 0},
	{"current", 0},
	{"next", 0},
	{"barycentric", 0},
	{"texoff", 0},
	{"vcolora", 0},
	{"vcolorb", 0},
};
static trailprog_t trail_debug;

typedef struct trailvtx_s {
	vec4f_t     vertex;
	vec3_t      bary;
	float       texoff;
	byte        colora[4];
	byte        colorb[4];
} trailvtx_t;

static void
alloc_arrays (psystem_t *ps)
{
	if (ps->maxparticles > maxparticles) {
		maxparticles = ps->maxparticles;
		if (particleVertexArray)
			free (particleVertexArray);
		printf ("alloc_arrays: %d\n", ps->maxparticles);
		particleVertexArray = calloc (ps->maxparticles * 4,
									  sizeof (partvert_t));

		if (pVAindices)
			free (pVAindices);
		pVAindices = calloc (ps->maxparticles * 6, sizeof (GLushort));
		for (uint32_t i = 0; i < ps->maxparticles; i++) {
			pVAindices[i * 6 + 0] = i * 4 + 0;
			pVAindices[i * 6 + 1] = i * 4 + 1;
			pVAindices[i * 6 + 2] = i * 4 + 2;
			pVAindices[i * 6 + 3] = i * 4 + 0;
			pVAindices[i * 6 + 4] = i * 4 + 2;
			pVAindices[i * 6 + 5] = i * 4 + 3;
		}
	}
}

void
glsl_R_ShutdownTrails (void)
{
	free (particleVertexArray);
	free (pVAindices);
}

static void
build_program (trailprog_t *prog, const char *name, int vert, int frag)
{
	prog->program = GLSL_LinkProgram (name, vert, frag);
	GLSL_ResolveShaderParam (prog->program, &prog->proj);
	GLSL_ResolveShaderParam (prog->program, &prog->view);
	GLSL_ResolveShaderParam (prog->program, &prog->viewport);
	GLSL_ResolveShaderParam (prog->program, &prog->width);
	GLSL_ResolveShaderParam (prog->program, &prog->last);
	GLSL_ResolveShaderParam (prog->program, &prog->current);
	GLSL_ResolveShaderParam (prog->program, &prog->next);
	GLSL_ResolveShaderParam (prog->program, &prog->barycentric);
	GLSL_ResolveShaderParam (prog->program, &prog->texoff);
	GLSL_ResolveShaderParam (prog->program, &prog->colora);
	GLSL_ResolveShaderParam (prog->program, &prog->colorb);
}

void
glsl_R_InitTrails (void)
{
	shader_t   *vert_shader, *frag_shader, *debug_shader;
	int         vert;
	int         frag;
	int         debug;


	vert_shader = GLSL_BuildShader (particle_trail_vert_effects);
	frag_shader = GLSL_BuildShader (particle_trail_frag_effects);
	debug_shader = GLSL_BuildShader (particle_trail_debug_frag_effects);
	vert = GLSL_CompileShader ("trail.vert", vert_shader, GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("trail.frag", frag_shader, GL_FRAGMENT_SHADER);
	debug = GLSL_CompileShader ("trail.frag.debug", debug_shader,
								GL_FRAGMENT_SHADER);
	trail_debug = trail;
	build_program (&trail, "trail", vert, frag);
	build_program (&trail_debug, "trail.debug", vert, debug);

	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);
	GLSL_FreeShader (debug_shader);

	alloc_arrays (&r_psystem);
}

static int
count_bits (uint32_t v)
{
	int         c = 0;
	for (; v; c++) {
		v &= v - 1;
	}
	return c;
}


static int
count_points (uint32_t block_mask)
{
	int         num_blocks = count_bits (block_mask);
	return num_blocks * 64;
}

static void
build_verts (trailvtx_t *verts, int *counts,
			 const trailpnt_t *points, uint32_t block_mask)
{
	uint32_t    id = ~0u;
	for (int m = 0; m < 32; m++, points += 64) {
		if (!(block_mask & (1 << m))) {
			if (*counts) {
				counts++;
			}
			continue;
		}
		if (id != ~0u && points->trailid != id) {
			counts++;
		}
		id = points->trailid;
		*counts += 64;
		for (int i = 0; i < 64; i++) {
			verts[i] = (trailvtx_t) {
				.vertex = points[i].pos,
				.bary = {VectorExpand (points[i].bary)},
				.texoff = points[i].pathoffset,
				.colora = { QuatExpand (points[i].colora)},
				.colorb = { QuatExpand (points[i].colorb)},
			};
		}
		verts += 64;
	}
}

static void
draw_trails (trailpnt_t *points, uint32_t block_mask)
{
	int         num_verts = count_points (block_mask);
	if (!num_verts) {
		return;
	}

	trailvtx_t  verts[num_verts];
	int         counts[33] = {};
	build_verts (verts, counts, points, block_mask);

	auto prog = trail;
	qfeglUseProgram (prog.program);
	qfeglEnableVertexAttribArray (prog.last.location);
	qfeglEnableVertexAttribArray (prog.current.location);
	qfeglEnableVertexAttribArray (prog.next.location);
	if (prog.barycentric.location >= 0)
		qfeglEnableVertexAttribArray (prog.barycentric.location);
	qfeglEnableVertexAttribArray (prog.texoff.location);
	if (prog.colora.location >= 0)
		qfeglEnableVertexAttribArray (prog.colora.location);
	if (prog.colorb.location >= 0)
	qfeglEnableVertexAttribArray (prog.colorb.location);

	qfeglUniformMatrix4fv (prog.proj.location, 1, false,
						   (vec_t*)&glsl_projection[0]);//FIXME
	qfeglUniformMatrix4fv (prog.view.location, 1, false,
						   (vec_t*)&glsl_view[0]);//FIXME
	qfeglUniform2f (prog.viewport.location, r_refdef.vrect.width,
					r_refdef.vrect.height);
	qfeglUniform1f (prog.width.location, 10);

	qfeglVertexAttribPointer (prog.last.location, 4, GL_FLOAT,
							 0, sizeof (trailvtx_t), &verts[0].vertex);
	qfeglVertexAttribPointer (prog.current.location, 4, GL_FLOAT,
							 0, sizeof (trailvtx_t), &verts[2].vertex);
	qfeglVertexAttribPointer (prog.next.location, 4, GL_FLOAT,
							 0, sizeof (trailvtx_t), &verts[4].vertex);
	if (prog.barycentric.location >= 0)
		qfeglVertexAttribPointer (prog.barycentric.location, 3, GL_FLOAT,
								 0, sizeof (trailvtx_t), &verts[2].bary);
	qfeglVertexAttribPointer (prog.texoff.location, 1, GL_FLOAT,
							 0, sizeof (trailvtx_t), &verts[0].texoff);
	if (prog.colora.location >= 0)
		qfeglVertexAttribPointer (prog.colora.location, 4, GL_UNSIGNED_BYTE,
								 1, sizeof (trailvtx_t), &verts[0].colora);
	if (prog.colorb.location >= 0)
		qfeglVertexAttribPointer (prog.colorb.location, 4, GL_UNSIGNED_BYTE,
								 1, sizeof (trailvtx_t), &verts[0].colorb);

	int         first = 0;
	for (auto c = counts; *c; c++) {
		qfeglDrawArrays (GL_TRIANGLE_STRIP, first, *c - 4);
		first += *c;
	}

	qfeglDisableVertexAttribArray (prog.last.location);
	qfeglDisableVertexAttribArray (prog.current.location);
	qfeglDisableVertexAttribArray (prog.next.location);
	if (prog.barycentric.location >= 0)
		qfeglDisableVertexAttribArray (prog.barycentric.location);
	qfeglDisableVertexAttribArray (prog.texoff.location);
	if (prog.colora.location >= 0)
		qfeglDisableVertexAttribArray (prog.colora.location);
	if (prog.colorb.location >= 0)
		qfeglDisableVertexAttribArray (prog.colorb.location);
}

void
glsl_R_DrawTrails (psystem_t *psystem)
{
	//FIXME abuse of psystem_t
	draw_trails ((trailpnt_t *)psystem->particles, psystem->numparticles);
}

psystem_t * __attribute__((const))//FIXME
glsl_TrailSystem (void)
{
	return &r_tsystem;
}
