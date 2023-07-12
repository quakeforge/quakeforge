/*
	glsl_particles.c

	OpenGL particle system.

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
//#include "QF/GL/qf_explosions.h"
#include "QF/GLSL/qf_particles.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_internal.h"

//FIXME not part of GLES, but needed for GL
#ifndef GL_VERTEX_PROGRAM_POINT_SIZE
# define GL_VERTEX_PROGRAM_POINT_SIZE 0x8642
#endif

static uint32_t     maxparticles;
static GLushort    *pVAindices;
static partvert_t  *particleVertexArray;

static GLuint   part_tex;

static const char *particle_point_vert_effects[] =
{
	"QuakeForge.Vertex.particle.point",
	0
};

static const char *particle_point_frag_effects[] =
{
	"QuakeForge.Fragment.fog",
	"QuakeForge.Fragment.palette",
	"QuakeForge.Fragment.particle.point",
	0
};

static const char *particle_textured_vert_effects[] =
{
	"QuakeForge.Vertex.particle.textured",
	0
};

static const char *particle_textured_frag_effects[] =
{
	"QuakeForge.Fragment.fog",
	"QuakeForge.Fragment.palette",
	"QuakeForge.Fragment.particle.textured",
	0
};

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
	shaderparam_t mvp_matrix;
	shaderparam_t vertex;
	shaderparam_t palette;
	shaderparam_t color;
	shaderparam_t fog;
} quake_point = {
	0,
	{"mvp_mat", 1},
	{"vertex", 0},
	{"palette", 1},
	{"vcolor", 0},
	{"fog", 1},
};

static struct {
	int         program;
	shaderparam_t mvp_matrix;
	shaderparam_t st;
	shaderparam_t vertex;
	shaderparam_t color;
	shaderparam_t texture;
	shaderparam_t fog;
} quake_part = {
	0,
	{"mvp_mat", 1},
	{"vst", 0},
	{"vertex", 0},
	{"vcolor", 0},
	{"texture", 1},
	{"fog", 1},
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
#if 0
typedef struct trail_s {
	struct trail_s *next;
	struct trail_s **prev;
	struct trail_s **owner;
	particle_t *points;
	particle_t **head;	// new points are appended to the list
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
ALLOC_STATE (particle_t, points);
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

static particle_t *
new_point (void)
{
	particle_t  *point;
	ALLOC (128, particle_t, points, point);
	return point;
}

static void
free_point (particle_t *point)
{
	FREE (points, point);
}
#endif
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

static void
glsl_particles_f (void *data, const cvar_t *cvar)
{
	alloc_arrays (&r_psystem);//FIXME
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
	float       v[2] = {0, 0};
	byte        data[64][64][2];
	tex_t      *tex;
#if 0
	R_LoadParticles ();
#endif
	Cvar_AddListener (Cvar_FindVar ("r_particles"), glsl_particles_f, 0);
	Cvar_AddListener (Cvar_FindVar ("r_particles_max"), glsl_particles_f, 0);

	qfeglEnable (GL_VERTEX_PROGRAM_POINT_SIZE);
	qfeglGetFloatv (GL_ALIASED_POINT_SIZE_RANGE, v);
	Sys_MaskPrintf (SYS_glsl, "point size: %g - %g\n", v[0], v[1]);

	vert_shader = GLSL_BuildShader (particle_point_vert_effects);
	frag_shader = GLSL_BuildShader (particle_point_frag_effects);
	vert = GLSL_CompileShader ("quakepnt.vert", vert_shader,
							   GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("quakepnt.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	quake_point.program = GLSL_LinkProgram ("quakepoint", vert, frag);
	GLSL_ResolveShaderParam (quake_point.program, &quake_point.mvp_matrix);
	GLSL_ResolveShaderParam (quake_point.program, &quake_point.vertex);
	GLSL_ResolveShaderParam (quake_point.program, &quake_point.palette);
	GLSL_ResolveShaderParam (quake_point.program, &quake_point.color);
	GLSL_ResolveShaderParam (quake_point.program, &quake_point.fog);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);

	vert_shader = GLSL_BuildShader (particle_textured_vert_effects);
	frag_shader = GLSL_BuildShader (particle_textured_frag_effects);
	vert = GLSL_CompileShader ("quakepar.vert", vert_shader,
							   GL_VERTEX_SHADER);
	frag = GLSL_CompileShader ("quakepar.frag", frag_shader,
							   GL_FRAGMENT_SHADER);
	quake_part.program = GLSL_LinkProgram ("quakepart", vert, frag);
	GLSL_ResolveShaderParam (quake_part.program, &quake_part.mvp_matrix);
	GLSL_ResolveShaderParam (quake_part.program, &quake_part.st);
	GLSL_ResolveShaderParam (quake_part.program, &quake_part.vertex);
	GLSL_ResolveShaderParam (quake_part.program, &quake_part.color);
	GLSL_ResolveShaderParam (quake_part.program, &quake_part.texture);
	GLSL_ResolveShaderParam (quake_part.program, &quake_part.fog);
	GLSL_FreeShader (vert_shader);
	GLSL_FreeShader (frag_shader);

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

	memset (data, 0, sizeof (data));
	qfeglGenTextures (1, &part_tex);
	qfeglBindTexture (GL_TEXTURE_2D, part_tex);
	qfeglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	qfeglTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	qfeglTexImage2D (GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 64, 64, 0,
					GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, data);
	tex = R_DotParticleTexture ();
	qfeglTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, 32, 32, GL_LUMINANCE_ALPHA,
					   GL_UNSIGNED_BYTE, tex->data);
	free (tex);
	tex = R_SparkParticleTexture ();
	qfeglTexSubImage2D (GL_TEXTURE_2D, 0, 32, 0, 32, 32, GL_LUMINANCE_ALPHA,
					   GL_UNSIGNED_BYTE, tex->data);
	free (tex);
	tex = R_SmokeParticleTexture ();
	qfeglTexSubImage2D (GL_TEXTURE_2D, 0, 0, 32, 32, 32, GL_LUMINANCE_ALPHA,
					   GL_UNSIGNED_BYTE, tex->data);
	free (tex);

	alloc_arrays (&r_psystem);
}
#if 0
static void
add_trail_to_ent (entity_t *ent)
{
	ent->trail = new_trail ();
	ent->trail->next = trails_active;
	ent->trail->prev = &trails_active;
	ent->trail->owner = &ent->trail;
	if (trails_active)
		trails_active->prev = &ent->trail->next;
	trails_active = ent->trail;
}

static inline particle_t *
new_trail_point (const vec3_t origin, float pscale, float percent)
{
	particle_t *point;
	int         ramp;

	point = new_point ();
	VectorCopy (origin, point->org);
	point->color = ramp3[ramp = mtwist_rand (&mt) & 3];
	point->tex = part_tex_smoke;
	point->scale = pscale + percent * 4.0;
	point->alpha = 0.5 + qfrandom (0.125) - percent * 0.40;
	VectorCopy (vec3_origin, point->vel);
	point->die = vr_data.realtime + 2.0 - percent * 2.0;
	point->ramp = ramp;
	point->physics = R_ParticlePhysics ("pt_float");

	return point;
}

static inline void
add_point_to_trail (trail_t *trail, particle_t *point)
{
	*trail->head = point;
	trail->head = &point->next;
	trail->num_points++;
}

static void
R_RocketTrail_trail (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		old_origin, vec;
	particle_t *point;

	if (!ent->trail) {
		add_trail_to_ent (ent);
	}

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = vr_data.frametime / maxlen;
	pscale = 1.5 + qfrandom (1.5);

	while (len < maxlen) {
		pscalenext = 1.5 + qfrandom (1.5);
		dist = (pscale + pscalenext) * 3.0;
		percent = len * origlen;

		point = new_trail_point (old_origin, pscale, percent);

		add_point_to_trail (ent->trail, point);

		len += dist;
		VectorMultAdd (old_origin, len, vec, old_origin);
		pscale = pscalenext;
	}
}

static inline void
set_vertex (trailvtx_t *v, const particle_t *point, float w, const vec3_t bary,
			float off)
{
	byte       *color = (byte *) &d_8to24table[(byte) point->color];

	VectorCopy (point->org, v->vertex);
	VectorCopy (bary, v->bary);
	v->vertex[3] = w * point->scale;
	v->texoff = off;
	VectorScale (color, 1.5 / 255, v->colora);
	v->colora[3] = point->alpha * 1.2;
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
		// shader has valid data (even though the distance between points will
		// be 0), thus need vertices for two extra points.
		*num_verts += (trail->num_points + 2) * 2;
	}
}

static void
build_verts (trailvtx_t *v)
{
	trail_t    *trail;
	particle_t *point, *last_point = 0, *second_last_point = 0;
	particle_t  dup;
	float       bary[] = {0, 0, 1, 0, 0, 1, 0, 0};
	int         bind = 0;

	memset (&dup, 0, sizeof (dup));
	for (trail = trails_active; trail; trail = trail->next) {
		int         index = 0;
		if (trail->num_points < 2)
			continue;
		point = trail->points;
		VectorScale (point->org, 2, dup.org);
		VectorSubtract (dup.org, point->next->org, dup.org);
		set_vertex (v++, &dup, -1, bary, 0);
		set_vertex (v++, &dup, +1, bary, 0);
		for (point = trail->points; point; point = point->next) {
			second_last_point = last_point;
			last_point = point;
			set_vertex (v++, point, -1, bary + bind++, index);
			set_vertex (v++, point, +1, bary + bind++, index);
			bind %= 3;
			index++;
			R_RunParticlePhysics (point);
		}
		VectorScale (last_point->org, 2, dup.org);
		VectorSubtract (dup.org, second_last_point->org, dup.org);
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
		while (trail->points && trail->points->die <= vr_data.realtime) {
			particle_t *point = trail->points;
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

	qfeglUniformMatrix4fv (trail.proj.location, 1, false, glsl_projection);
	qfeglUniformMatrix4fv (trail.view.location, 1, false, glsl_view);
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
#endif
static void
draw_qf_particles (psystem_t *psystem)
{
	byte       *at;
	int         vacount;
	float       minparticledist, scale;
	vec3_t      up_scale, right_scale, up_right_scale, down_right_scale;
	partvert_t *VA;
	mat4f_t     vp_mat;
	quat_t      fog;

	mmulf (vp_mat, glsl_projection, glsl_view);

	qfeglDepthMask (GL_FALSE);
	qfeglUseProgram (quake_part.program);
	qfeglEnableVertexAttribArray (quake_part.vertex.location);
	qfeglEnableVertexAttribArray (quake_part.color.location);
	qfeglEnableVertexAttribArray (quake_part.st.location);

	Fog_GetColor (fog);
	fog[3] = Fog_GetDensity () / 64.0;
	qfeglUniform4fv (quake_part.fog.location, 1, fog);

	qfeglUniformMatrix4fv (quake_part.mvp_matrix.location, 1, false,
						   (vec_t*)&vp_mat[0]);//FIXME

	qfeglUniform1i (quake_part.texture.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, part_tex);

	// LordHavoc: particles should not affect zbuffer
	qfeglDepthMask (GL_FALSE);

	minparticledist = DotProduct (r_refdef.frame.position,
								  r_refdef.frame.forward)
		+ r_particles_nearclip;

	vacount = 0;
	VA = particleVertexArray;

	for (unsigned i = 0; i < psystem->numparticles; i++) {
		particle_t *p = &psystem->particles[i];
		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (p->pos, r_refdef.frame.forward) < minparticledist)) {
			at = (byte *) &d_8to24table[(byte) p->icolor];
			VA[0].color[0] = at[0];
			VA[0].color[1] = at[1];
			VA[0].color[2] = at[2];
			VA[0].color[3] = p->alpha * 255;
			memcpy (VA[1].color, VA[0].color, sizeof (VA[0].color));
			memcpy (VA[2].color, VA[0].color, sizeof (VA[0].color));
			memcpy (VA[3].color, VA[0].color, sizeof (VA[0].color));

			switch (p->tex) {
				case part_tex_dot:
					VA[0].texcoord[0] = 0.0;
					VA[0].texcoord[1] = 0.0;
					VA[1].texcoord[0] = 0.5;
					VA[1].texcoord[1] = 0.0;
					VA[2].texcoord[0] = 0.5;
					VA[2].texcoord[1] = 0.5;
					VA[3].texcoord[0] = 0.0;
					VA[3].texcoord[1] = 0.5;
					break;
				case part_tex_spark:
					VA[0].texcoord[0] = 0.5;
					VA[0].texcoord[1] = 0.0;
					VA[1].texcoord[0] = 1.0;
					VA[1].texcoord[1] = 0.0;
					VA[2].texcoord[0] = 1.0;
					VA[2].texcoord[1] = 0.5;
					VA[3].texcoord[0] = 0.5;
					VA[3].texcoord[1] = 0.5;
					break;
				case part_tex_smoke:
					VA[0].texcoord[0] = 0.0;
					VA[0].texcoord[1] = 0.5;
					VA[1].texcoord[0] = 0.5;
					VA[1].texcoord[1] = 0.5;
					VA[2].texcoord[0] = 0.5;
					VA[2].texcoord[1] = 1.0;
					VA[3].texcoord[0] = 0.0;
					VA[3].texcoord[1] = 1.0;
					break;
			}

			scale = p->scale;

			VectorScale (r_refdef.frame.up, scale, up_scale);
			VectorScale (r_refdef.frame.right, scale, right_scale);

			VectorAdd (right_scale, up_scale, up_right_scale);
			VectorSubtract (right_scale, up_scale, down_right_scale);

			VectorAdd (p->pos, down_right_scale, VA[0].vertex);
			VectorSubtract (p->pos, up_right_scale, VA[1].vertex);
			VectorSubtract (p->pos, down_right_scale, VA[2].vertex);
			VectorAdd (p->pos, up_right_scale, VA[3].vertex);

			VA += 4;
			vacount += 6;
		}
	}

	qfeglVertexAttribPointer (quake_part.vertex.location, 3, GL_FLOAT,
							 0, sizeof (partvert_t),
							 &particleVertexArray[0].vertex);
	qfeglVertexAttribPointer (quake_part.color.location, 4, GL_UNSIGNED_BYTE,
							 1, sizeof (partvert_t),
							 &particleVertexArray[0].color);
	qfeglVertexAttribPointer (quake_part.st.location, 2, GL_FLOAT,
							 0, sizeof (partvert_t),
							 &particleVertexArray[0].texcoord);
	qfeglDrawElements (GL_TRIANGLES, vacount, GL_UNSIGNED_SHORT, pVAindices);

	qfeglDepthMask (GL_TRUE);
	qfeglDisableVertexAttribArray (quake_part.vertex.location);
	qfeglDisableVertexAttribArray (quake_part.color.location);
	qfeglDisableVertexAttribArray (quake_part.st.location);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
}

static void
draw_id_particles (psystem_t *psystem)
{
	int         vacount;
	float       minparticledist;
	partvert_t *VA;
	mat4f_t     vp_mat;
	quat_t      fog;

	mmulf (vp_mat, glsl_projection, glsl_view);

	// LordHavoc: particles should not affect zbuffer
	qfeglDepthMask (GL_FALSE);
	qfeglUseProgram (quake_point.program);
	qfeglEnableVertexAttribArray (quake_point.vertex.location);
	qfeglEnableVertexAttribArray (quake_point.color.location);

	qfeglUniformMatrix4fv (quake_point.mvp_matrix.location, 1, false,
						   (vec_t*)&vp_mat[0]);//FIXME

	Fog_GetColor (fog);
	fog[3] = Fog_GetDensity () / 64.0;
	qfeglUniform4fv (quake_point.fog.location, 1, fog);

	qfeglUniform1i (quake_point.palette.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, glsl_palette);

	minparticledist = DotProduct (r_refdef.frame.position,
								  r_refdef.frame.forward)
		+ r_particles_nearclip;

	vacount = 0;
	VA = particleVertexArray;

	for (unsigned i = 0; i < psystem->numparticles; i++) {
		particle_t *p = &psystem->particles[i];
		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (p->pos, r_refdef.frame.forward) < minparticledist)) {
			VA[0].color[0] = (byte) p->icolor;
			VectorCopy (p->pos, VA[0].vertex);
			VA++;
			vacount++;
		}
	}

	qfeglVertexAttribPointer (quake_point.vertex.location, 3, GL_FLOAT,
							 0, sizeof (partvert_t),
							 &particleVertexArray[0].vertex);
	qfeglVertexAttribPointer (quake_point.color.location, 1, GL_UNSIGNED_BYTE,
							 1, sizeof (partvert_t),
							 &particleVertexArray[0].color);
	qfeglDrawArrays (GL_POINTS, 0, vacount);

	qfeglDepthMask (GL_TRUE);
	qfeglDisableVertexAttribArray (quake_point.vertex.location);
	qfeglDisableVertexAttribArray (quake_point.color.location);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
}

void
glsl_R_DrawParticles (psystem_t *psystem)
{
#if 0
	draw_trails ();
#endif
	if (!psystem->numparticles) {
		return;
	}
	if (!psystem->points_only) {
		draw_qf_particles (psystem);
	} else {
		draw_id_particles (psystem);
	}
}

psystem_t * __attribute__((const))//FIXME
glsl_ParticleSystem (void)
{
	return &r_psystem;
}
