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

static struct {
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
} trail = {
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

typedef struct point_s {
	particle_t  p;
	struct point_s *next;
} point_t;

typedef struct trail_s {
	struct trail_s *next;
	struct trail_s **prev;
	struct trail_s **owner;
	point_t    *points;
	point_t   **head;	// new points are appended to the list
	int         num_points;
} trail_t;

typedef struct trailvtx_s {
	quat_t      vertex;
	vec3_t      bary;
	float       texoff;
	quat_t      colora;
	quat_t      colorb;
} trailvtx_t;

ALLOC_STATE (trail_t, trails);
ALLOC_STATE (point_t, points);
static trail_t *trails_active;

static trail_t *
new_trail (void)
{
	trail_t    *trail;
	ALLOC (16, trail_t, trails, trail);
	trail->head = &trail->points;
	return trail;
}

static void
free_trail (trail_t *trail)
{
	FREE (trails, trail);
}

static point_t *
new_point (void)
{
	point_t    *point;
	ALLOC (128, point_t, points, point);
	return point;
}

static void
free_point (point_t *point)
{
	FREE (points, point);
}

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
glsl_R_ShutdownParticles (void)
{
	free (particleVertexArray);
	free (pVAindices);
}

void
glsl_R_InitParticles (void)
{
	shader_t   *vert_shader, *frag_shader;
	int         vert;
	int         frag;

	vert_shader = GLSL_BuildShader (particle_trail_vert_effects);
	frag_shader = GLSL_BuildShader (particle_trail_frag_effects);
	vert = GLSL_CompileShader ("trail.vert", vert_shader, GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("trail.frag", frag_shader, GL_FRAGMENT_SHADER);
	trail.program = GLSL_LinkProgram ("trail", vert, frag);
	GLSL_ResolveShaderParam (trail.program, &trail.proj);
	GLSL_ResolveShaderParam (trail.program, &trail.view);
	GLSL_ResolveShaderParam (trail.program, &trail.viewport);
	GLSL_ResolveShaderParam (trail.program, &trail.width);
	GLSL_ResolveShaderParam (trail.program, &trail.last);
	GLSL_ResolveShaderParam (trail.program, &trail.current);
	GLSL_ResolveShaderParam (trail.program, &trail.next);
	GLSL_ResolveShaderParam (trail.program, &trail.barycentric);
	GLSL_ResolveShaderParam (trail.program, &trail.texoff);
	GLSL_ResolveShaderParam (trail.program, &trail.colora);
	GLSL_ResolveShaderParam (trail.program, &trail.colorb);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);

	alloc_arrays (&r_psystem);
}

static void
add_trail_to_ent (entity_t ent)
{
	renderer_t *r = Ent_GetComponent (ent.id, scene_renderer, ent.reg);
	r->trail = new_trail ();
	r->trail->next = trails_active;
	r->trail->prev = &trails_active;
	r->trail->owner = &r->trail;
	if (trails_active)
		trails_active->prev = &r->trail->next;
	trails_active = r->trail;
}

static inline point_t *
new_trail_point (vec4f_t origin, float pscale, float percent)
{
	point_t    *point;
	//int         ramp;

	point = new_point ();
	*point = (point_t) {
		.p.pos = origin,
		.p.color = (vec4f_t) {1,1,1,1},//ramp3[ramp = mtwist_rand (&mt) & 3],XXX
		.p.tex = part_tex_smoke,
		.p.scale = pscale + percent * 4.0,
		.p.alpha = 0.5 + qfrandom (0.125) - percent * 0.40,
		.p.vel = {},
		.p.live = 2.0 - percent * 2.0,
	//	.p.ramp = ramp,XXX
	//	.p.physics = R_ParticlePhysics ("pt_float"),XXX
	};

	return point;
}

static inline void
add_point_to_trail (trail_t *trail, point_t *point)
{
	*trail->head = point;
	trail->head = &point->next;
	trail->num_points++;
}

static void __attribute__((used))//XXX
R_RocketTrail_trail (entity_t ent)
{
	transform_t transform = Entity_Transform (ent);
	renderer_t  *renderer = Ent_GetComponent (ent.id, scene_renderer, ent.reg);
	vec4f_t    *old_origin = Ent_GetComponent (ent.id, scene_old_origin, ent.reg);
	vec4f_t     ent_origin = Transform_GetWorldPosition (transform);

	vec4f_t     vec = ent_origin - *old_origin;
	float       maxlen = magnitudef (vec)[0];
	vec /= maxlen;

	if (!renderer->trail) {
		add_trail_to_ent (ent);
	}
	trail_t    *trail = renderer->trail;

	float       origlen = vr_data.frametime / maxlen;
	float       pscale = 1.5 + qfrandom (1.5);
	vec4f_t     pos = *old_origin;
	float		len = 0.0;

	while (len < maxlen) {
		float       pscalenext = 1.5 + qfrandom (1.5);
		float       dist = (pscale + pscalenext) * 3.0;
		float       percent = len * origlen;

		point_t    *point = new_trail_point (pos, pscale, percent);
		add_point_to_trail (trail, point);

		len += dist;
		pos += dist * vec;
		pscale = pscalenext;
	}
}

static inline void
set_vertex (trailvtx_t *v, const point_t *point, float w, const vec3_t bary,
			float off)
{
	VectorCopy (point->p.pos, v->vertex);
	VectorCopy (bary, v->bary);
	v->vertex[3] = w * point->p.scale;
	v->texoff = off;
	VectorCopy (point->p.color, v->colora);
	v->colora[3] = point->p.alpha * 1.2;
	QuatSet (-1, -1, -1, -0.5, v->colorb);
}

static void
count_points (int *num_verts)
{
	trail_t    *trail;

	*num_verts = 0;
	for (trail = trails_active; trail; trail = trail->next) {
		if (trail->num_points < 2)
			continue;
		// Each point has two vertices, thus the * 2. However, both the
		// first point and the last point need to be duplicated so the vertex
		// shader has valid data, thus need vertices for two extra points.
		*num_verts += (trail->num_points + 2) * 2;
	}
}

static void
build_verts (trailvtx_t *v)
{
	trail_t    *trail;
	point_t    *point, *last_point = 0, *second_last_point = 0;
	point_t     dup;
	float       bary[] = {0, 0, 1, 0, 0, 1, 0, 0};
	int         bind = 0;

	memset (&dup, 0, sizeof (dup));
	for (trail = trails_active; trail; trail = trail->next) {
		int         index = 0;
		if (trail->num_points < 2)
			continue;
		point = trail->points;
		dup.p.pos = 2 * point->p.pos;
		dup.p.pos -= point->next->p.pos;
		set_vertex (v++, &dup, -1, bary, 0);
		set_vertex (v++, &dup, +1, bary, 0);
		for (point = trail->points; point; point = point->next) {
			second_last_point = last_point;
			last_point = point;
			set_vertex (v++, point, -1, bary + bind++, index);
			set_vertex (v++, point, +1, bary + bind++, index);
			bind %= 3;
			index++;
			R_RunParticlePhysics (&point->p);
		}
		dup.p.pos = 2 * last_point->p.pos;
		dup.p.pos -= second_last_point->p.pos;
		set_vertex (v++, &dup, -1, bary, 0);
		set_vertex (v++, &dup, +1, bary, 0);
	}
}

static void
expire_trails (void)
{
	trail_t    *trail, *next_trail;

	for (trail = trails_active; trail; trail = next_trail) {
		next_trail = trail->next;
		while (trail->points && trail->points->p.live <= 0) {
			point_t    *point = trail->points;
			trail->points = point->next;
			free_point (point);
			trail->num_points--;
		}
		if (trail->num_points < 2) {
			if (trail->next)
				trail->next->prev = trail->prev;
			*trail->prev = trail->next;
			*trail->owner = 0;
			free_trail (trail);
		}
	}
}

static void
draw_trails (void)
{
	trail_t    *trl;
	int         num_verts;
	int         first;
	trailvtx_t *verts;

	count_points (&num_verts);
	if (!num_verts)
		return;

	verts = alloca (num_verts * sizeof (trailvtx_t));
	build_verts (verts);

	qfeglUseProgram (trail.program);
	qfeglEnableVertexAttribArray (trail.last.location);
	qfeglEnableVertexAttribArray (trail.current.location);
	qfeglEnableVertexAttribArray (trail.next.location);
	if (trail.barycentric.location >= 0)
		qfeglEnableVertexAttribArray (trail.barycentric.location);
	qfeglEnableVertexAttribArray (trail.texoff.location);
	qfeglEnableVertexAttribArray (trail.colora.location);
	qfeglEnableVertexAttribArray (trail.colorb.location);

	qfeglUniformMatrix4fv (trail.proj.location, 1, false,
						   (vec_t*)&glsl_projection[0]);//FIXME
	qfeglUniformMatrix4fv (trail.view.location, 1, false,
						   (vec_t*)&glsl_view[0]);//FIXME
	qfeglUniform2f (trail.viewport.location, r_refdef.vrect.width,
					r_refdef.vrect.height);
	qfeglUniform1f (trail.width.location, 10);

	qfeglVertexAttribPointer (trail.last.location, 4, GL_FLOAT,
							 0, sizeof (trailvtx_t), &verts[0].vertex);
	qfeglVertexAttribPointer (trail.current.location, 4, GL_FLOAT,
							 0, sizeof (trailvtx_t), &verts[2].vertex);
	qfeglVertexAttribPointer (trail.next.location, 4, GL_FLOAT,
							 0, sizeof (trailvtx_t), &verts[4].vertex);
	if (trail.barycentric.location >= 0)
		qfeglVertexAttribPointer (trail.barycentric.location, 3, GL_FLOAT,
								 0, sizeof (trailvtx_t), &verts[2].bary);
	qfeglVertexAttribPointer (trail.texoff.location, 1, GL_FLOAT,
							 0, sizeof (trailvtx_t), &verts[0].texoff);
	qfeglVertexAttribPointer (trail.colora.location, 4, GL_FLOAT,
							 0, sizeof (trailvtx_t), &verts[0].colora);
	qfeglVertexAttribPointer (trail.colorb.location, 4, GL_FLOAT,
							 0, sizeof (trailvtx_t), &verts[0].colorb);

	for (first = 0, trl = trails_active; trl; trl = trl->next) {
		int         count;
		if (trl->num_points < 2)
			continue;
		count = trl->num_points * 2;
		qfeglDrawArrays (GL_TRIANGLE_STRIP, first, count);
		first += count + 4;
	}

	qfeglDisableVertexAttribArray (trail.last.location);
	qfeglDisableVertexAttribArray (trail.current.location);
	qfeglDisableVertexAttribArray (trail.next.location);
	if (trail.barycentric.location >= 0)
		qfeglDisableVertexAttribArray (trail.barycentric.location);
	qfeglDisableVertexAttribArray (trail.texoff.location);
	qfeglDisableVertexAttribArray (trail.colora.location);
	qfeglDisableVertexAttribArray (trail.colorb.location);

	expire_trails ();
}

static void __attribute__((used))//XXX
glsl_R_DrawTrails (psystem_t *psystem)
{
	draw_trails ();
}

static psystem_t * __attribute__((const,used))//FIXME XXX
glsl_TrailSystem (void)
{
	return &r_psystem;
}
