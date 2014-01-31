/*
	gl_dyn_part.c

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

#define NH_DEFINE
#include "namehack.h"

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
#include "QF/GLSL/defines.h"
#include "QF/GLSL/funcs.h"
//#include "QF/GL/qf_explosions.h"
#include "QF/GLSL/qf_particles.h"
#include "QF/GLSL/qf_textures.h"
#include "QF/GLSL/qf_vid.h"

#include "r_internal.h"

static mtstate_t mt;	// private PRNG state

//FIXME not part of GLES, but needed for GL
#ifndef GL_VERTEX_PROGRAM_POINT_SIZE
# define GL_VERTEX_PROGRAM_POINT_SIZE 0x8642
#endif

//FIXME should not be here
static int  ramp1[8] = { 0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61 };
//static int  ramp2[8] = { 0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66 };
static int  ramp3[8] = { 0x6d, 0x6b, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01 };
static vec3_t vertex_normals[NUMVERTEXNORMALS] = {
#include "anorms.h"
};

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
	"QuakeForge.Vertex.particle.trail",
	0
};

static const char *particle_trail_frag_effects[] =
{
	"QuakeForge.Fragment.barycentric",
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
	shaderparam_t smoke;
} trail = {
	0,
	{"proj", 1},
	{"view", 1},
	{"viewport", 1},
	{"width", 1},
	{"last", 0},
	{"current", 0},
	{"next", 0},
	{"barycentric", 0},
	{"texoff", 0},
	{"smoke", 1},
};

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
} trailvtx_t;

static trail_t *trails_freelist;
static trail_t *trails_active;
static particle_t *points_freelist;

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

inline static void
particle_new (const char *type, int texnum, const vec3_t org, float scale,
			  const vec3_t vel, float die, int color, float alpha, float ramp)
{
	particle_t *part;

/*
	// Uncomment this for particle debugging!
	if (numparticles >= r_maxparticles) {
		Sys_Error  ("FAILED PARTICLE ALLOC!");
		return NULL;
	}
*/

	part = &particles[numparticles++];

	VectorCopy (org, part->org);
	part->color = color;
	part->tex = texnum;
	part->scale = scale;
	part->alpha = alpha;
	VectorCopy (vel, part->vel);
	part->die = die;
	part->ramp = ramp;
	part->physics = R_ParticlePhysics (type);
}

/*
	particle_new_random

	note that org_fuzz & vel_fuzz should be ints greater than 0 if you are
	going to bother using this function.
*/
inline static void
particle_new_random (const char *type, int texnum, const vec3_t org,
					 int org_fuzz, float scale, int vel_fuzz, float die,
					 int color, float alpha, float ramp)
{
	float		o_fuzz = org_fuzz, v_fuzz = vel_fuzz;
	int         rnd;
	vec3_t      porg, pvel;

	rnd = mtwist_rand (&mt);
	porg[0] = o_fuzz * ((rnd & 63) - 31.5) / 63.0 + org[0];
	porg[1] = o_fuzz * (((rnd >> 6) & 63) - 31.5) / 63.0 + org[1];
	porg[2] = o_fuzz * (((rnd >> 10) & 63) - 31.5) / 63.0 + org[2];
	rnd = mtwist_rand (&mt);
	pvel[0] = v_fuzz * ((rnd & 63) - 31.5) / 63.0;
	pvel[1] = v_fuzz * (((rnd >> 6) & 63) - 31.5) / 63.0;
	pvel[2] = v_fuzz * (((rnd >> 10) & 63) - 31.5) / 63.0;

	particle_new (type, texnum, porg, scale, pvel, die, color, alpha, ramp);
}

/*
inline static void
particle_new_veryrandom (ptype_t type, int texnum, const vec3_t org,
						 int org_fuzz, float scale, int vel_fuzz, float die,
						 int color, float alpha, float ramp)
{
	vec3_t		porg, pvel;

	porg[0] = qfrandom (org_fuzz * 2) - org_fuzz + org[0];
	porg[1] = qfrandom (org_fuzz * 2) - org_fuzz + org[1];
	porg[2] = qfrandom (org_fuzz * 2) - org_fuzz + org[2];
	pvel[0] = qfrandom (vel_fuzz * 2) - vel_fuzz;
	pvel[1] = qfrandom (vel_fuzz * 2) - vel_fuzz;
	pvel[2] = qfrandom (vel_fuzz * 2) - vel_fuzz;
	particle_new (type, texnum, porg, scale, pvel, die, color, alpha, ramp);
}
*/

void
glsl_R_ClearParticles (void)
{
	numparticles = 0;
}

void
glsl_R_InitParticles (void)
{
	shader_t   *vert_shader, *frag_shader;
	unsigned    i;
	int         vert;
	int         frag;
	float       v[2] = {0, 0};
	byte        data[64][64][2];
	tex_t      *tex;

	R_LoadParticles ();

	mtwist_seed (&mt, 0xdeadbeef);

	qfeglEnable (GL_VERTEX_PROGRAM_POINT_SIZE);
	qfeglGetFloatv (GL_ALIASED_POINT_SIZE_RANGE, v);
	Sys_MaskPrintf (SYS_GLSL, "point size: %g - %g\n", v[0], v[1]);

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
	GLSL_ResolveShaderParam (trail.program, &trail.smoke);
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

	if (particleVertexArray)
		free (particleVertexArray);
	particleVertexArray = calloc (r_maxparticles * 4, sizeof (partvert_t));

	if (pVAindices)
		free (pVAindices);
	pVAindices = calloc (r_maxparticles * 6, sizeof (GLushort));
	for (i = 0; i < r_maxparticles; i++) {
		pVAindices[i * 6 + 0] = i * 4 + 0;
		pVAindices[i * 6 + 1] = i * 4 + 1;
		pVAindices[i * 6 + 2] = i * 4 + 2;
		pVAindices[i * 6 + 3] = i * 4 + 0;
		pVAindices[i * 6 + 4] = i * 4 + 2;
		pVAindices[i * 6 + 5] = i * 4 + 3;
	}
}

void
glsl_R_ReadPointFile_f (void)
{
	const char *name;
	char       *mapname;
	int         c, r;
	vec3_t      org;
	QFile      *f;

	mapname = strdup (r_worldentity.model->name);
	if (!mapname)
		Sys_Error ("Can't duplicate mapname!");
	QFS_StripExtension (mapname, mapname);

	name = va ("%s.pts", mapname);
	free (mapname);

	f = QFS_FOpenFile (name);
	if (!f) {
		Sys_Printf ("couldn't open %s\n", name);
		return;
	}

	Sys_MaskPrintf (SYS_DEV, "Reading %s...\n", name);
	c = 0;
	for (;;) {
		char        buf[64];

		Qgets (f, buf, sizeof (buf));
		r = sscanf (buf, "%f %f %f\n", &org[0], &org[1], &org[2]);
		if (r != 3)
			break;
		c++;

		if (numparticles >= r_maxparticles) {
			Sys_MaskPrintf (SYS_DEV, "Not enough free particles\n");
			break;
		} else {
			particle_new ("pt_static", part_tex_dot, org, 1.5, vec3_origin,
						  99999, (-c) & 15, 1.0, 0.0);
		}
	}
	Qclose (f);
	Sys_MaskPrintf (SYS_DEV, "%i points read\n", c);
}

static void
R_ParticleExplosion_QF (const vec3_t org)
{
//	R_NewExplosion (org);
	if (numparticles >= r_maxparticles)
		return;
	particle_new_random ("pt_smokecloud", part_tex_smoke, org, 4, 30, 8,
						 vr_data.realtime + 5.0, (mtwist_rand (&mt) & 7) + 8,
						 0.5 + qfrandom (0.25), 0.0);
}

static void
R_ParticleExplosion2_QF (const vec3_t org, int colorStart, int colorLength)
{
	unsigned int	i, j = 512;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j; i++) {
		particle_new_random ("pt_blob", part_tex_dot, org, 16, 2, 256,
							 vr_data.realtime + 0.3,
							 colorStart + (i % colorLength), 1.0, 0.0);
	}
}

static void
R_BlobExplosion_QF (const vec3_t org)
{
	unsigned int	i;
	unsigned int	j = 1024;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j >> 1; i++) {
		particle_new_random ("pt_blob", part_tex_dot, org, 12, 2, 256,
							 vr_data.realtime + 1.0 + (mtwist_rand (&mt) & 7) * 0.05,
							 66 + i % 6, 1.0, 0.0);
	}
	for (i = 0; i < j / 2; i++) {
		particle_new_random ("pt_blob2", part_tex_dot, org, 12, 2, 256,
							 vr_data.realtime + 1.0 + (mtwist_rand (&mt) & 7) * 0.05,
							 150 + i % 6, 1.0, 0.0);
	}
}

static inline void
R_RunSparkEffect_QF (const vec3_t org, int count, int ofuzz)
{
	if (numparticles >= r_maxparticles)
		return;
	particle_new ("pt_smokecloud", part_tex_smoke, org, ofuzz * 0.08,
				  vec3_origin, vr_data.realtime + 9, 12 + (mtwist_rand (&mt) & 3),
				  0.25 + qfrandom (0.125), 0.0);

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	if (count) {
		int orgfuzz = ofuzz * 3 / 4;
		if (orgfuzz < 1)
			orgfuzz = 1;

		while (count--) {
			int		color = mtwist_rand (&mt) & 7;

			particle_new_random ("pt_fallfadespark", part_tex_dot, org, orgfuzz,
								 0.7, 96, vr_data.realtime + 5.0, ramp1[color],
								 1.0, color);
		}
	}
}

static inline void
R_BloodPuff_QF (const vec3_t org, int count)
{
	if (numparticles >= r_maxparticles)
		return;

	particle_new ("pt_bloodcloud", part_tex_smoke, org, count / 5, vec3_origin,
				  vr_data.realtime + 99.0, 70 + (mtwist_rand (&mt) & 3), 0.5, 0.0);
}

static void
R_BloodPuffEffect_QF (const vec3_t org, int count)
{
	R_BloodPuff_QF (org, count);
}

static void
R_GunshotEffect_QF (const vec3_t org, int count)
{
	int scale = 16;

	scale += count / 15;
	R_RunSparkEffect_QF (org, count >> 1, scale);
}

static void
R_LightningBloodEffect_QF (const vec3_t org)
{
	int		count = 7;

	R_BloodPuff_QF (org, 50);

	if (numparticles >= r_maxparticles)
		return;
	particle_new ("pt_smokecloud", part_tex_smoke, org, 3.0, vec3_origin,
				  vr_data.realtime + 9.0, 12 + (mtwist_rand (&mt) & 3),
				  0.25 + qfrandom (0.125), 0.0);

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	while (count--)
		particle_new_random ("pt_fallfade", part_tex_spark, org, 12, 2.0, 128,
							 vr_data.realtime + 5.0, 244 + (count % 3),
							 1.0, 0.0);
}

static void
R_RunParticleEffect_QF (const vec3_t org, const vec3_t dir, int color,
						int count)
{
	float       scale;
	int         i;
	vec3_t      porg;

	if (numparticles >= r_maxparticles)
		return;

	scale = pow (count, 0.23); // calculate scale before clipping to part. max

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;

	for (i = 0; i < count; i++) {
		int 	rnd = mtwist_rand (&mt);

		porg[0] = org[0] + scale * (((rnd >> 3) & 15) - 7.5);
		porg[1] = org[1] + scale * (((rnd >> 7) & 15) - 7.5);
		porg[2] = org[2] + scale * (((rnd >> 11) & 15) - 7.5);
		// Note that ParseParticleEffect handles (dir * 15)
		particle_new ("pt_grav", part_tex_dot, porg, 1.5, dir,
					  vr_data.realtime + 0.1 * (i % 5),
					  (color & ~7) + (rnd & 7), 1.0, 0.0);
	}
}

static void
R_SpikeEffect_QF (const vec3_t org)
{
	R_RunSparkEffect_QF (org, 5, 8);
}

static void
R_SuperSpikeEffect_QF (const vec3_t org)
{
	R_RunSparkEffect_QF (org, 10, 8);
}

static void
R_KnightSpikeEffect_QF (const vec3_t org)
{
	unsigned int	count = 10;

	if (numparticles >= r_maxparticles)
		return;
	particle_new ("pt_smokecloud", part_tex_smoke, org, 1.0, vec3_origin,
				  vr_data.realtime + 9.0, 234, 0.25 + qfrandom (0.125), 0.0);

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	while (count--)
		particle_new_random ("pt_fallfade", part_tex_dot, org, 6, 0.7, 96,
							 vr_data.realtime + 5.0, 234, 1.0, 0.0);
}

static void
R_WizSpikeEffect_QF (const vec3_t org)
{
	unsigned int	count = 15;

	if (numparticles >= r_maxparticles)
		return;
	particle_new ("pt_smokecloud", part_tex_smoke, org, 2.0, vec3_origin,
				  vr_data.realtime + 9.0, 63, 0.25 + qfrandom (0.125), 0.0);

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;
	while (count--)
		particle_new_random ("pt_fallfade", part_tex_dot, org, 12, 0.7, 96,
							 vr_data.realtime + 5.0, 63, 1.0, 0.0);
}

static void
R_LavaSplash_QF (const vec3_t org)
{
	float       vel;
	int         rnd, i, j;
	int			k = 256;
	vec3_t      dir, porg, pvel;

	if (numparticles + k >= r_maxparticles) {
		return;
	} // else if (numparticles + k >= r_maxparticles) {
//		k = r_maxparticles - numparticles;
//	}

	dir[2] = 256;
	for (i = -128; i < 128; i += 16) {
		for (j = -128; j < 128; j += 16) {
			rnd = mtwist_rand (&mt);
			dir[0] = j + (rnd & 7);
			dir[1] = i + ((rnd >> 6) & 7);

			porg[0] = org[0] + dir[0];
			porg[1] = org[1] + dir[1];
			porg[2] = org[2] + ((rnd >> 9) & 63);

			VectorNormalize (dir);
			rnd = mtwist_rand (&mt);
			vel = 50.0 + 0.5 * (float) (rnd & 127);
			VectorScale (dir, vel, pvel);
			particle_new ("pt_grav", part_tex_dot, porg, 3, pvel,
						  vr_data.realtime + 2.0 + ((rnd >> 7) & 31) * 0.02,
						  224 + ((rnd >> 12) & 7), 0.75, 0.0);
		}
	}
}

static void
R_TeleportSplash_QF (const vec3_t org)
{
	float       vel;
	int         rnd, i, j, k;
	int			l = 896;
	vec3_t      dir, pdir, porg, pvel;

	if (numparticles + l >= r_maxparticles) {
		return;
	} // else if (numparticles + l >= r_maxparticles) {
//		l = r_maxparticles - numparticles;
//	}

	for (k = -24; k < 32; k += 4) {
		dir[2] = k * 8;
		for (i = -16; i < 16; i += 4) {
			dir[1] = i * 8;
			for (j = -16; j < 16; j += 4) {
				dir[0] = j * 8;

				VectorCopy (dir, pdir);
				VectorNormalize (pdir);

				rnd = mtwist_rand (&mt);
				porg[0] = org[0] + i + (rnd & 3);
				porg[1] = org[1] + j + ((rnd >> 2) & 3);
				porg[2] = org[2] + k + ((rnd >> 4) & 3);

				vel = 50 + ((rnd >> 6) & 63);
				VectorScale (pdir, vel, pvel);
				particle_new ("pt_grav", part_tex_spark, porg, 0.6, pvel,
							  (vr_data.realtime + 0.2 + (mtwist_rand (&mt) & 15) * 0.01),
							  (7 + ((rnd >> 12) & 7)), 1.0, 0.0);
			}
		}
	}
}

static void
R_RocketTrail_trail (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		old_origin, vec;
	particle_t *point;

	if (!ent->trail) {
		ent->trail = new_trail ();
		ent->trail->next = trails_active;
		ent->trail->prev = &trails_active;
		ent->trail->owner = &ent->trail;
		if (trails_active)
			trails_active->prev = &ent->trail->next;
		trails_active = ent->trail;
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

		point = new_point ();
		VectorCopy (old_origin, point->org);
		point->color = 12 + (mtwist_rand (&mt) & 3);
		point->tex = part_tex_smoke;
		point->scale = pscale + percent * 4.0;
		point->alpha = 0.5 + qfrandom (0.125) - percent * 0.40;
		VectorCopy (vec3_origin, point->vel);
		point->die = vr_data.realtime + 2.0 - percent * 2.0;
		point->ramp = 0.0;
		point->physics = R_ParticlePhysics ("pt_fire");

		*ent->trail->head = point;
		ent->trail->head = &point->next;
		ent->trail->num_points++;

		len += dist;
		VectorMultAdd (old_origin, len, vec, old_origin);
		pscale = pscalenext;
	}
}

static void
R_RocketTrail_QF (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		old_origin, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = vr_data.frametime / maxlen;
	pscale = 1.5 + qfrandom (1.5);

	while (len < maxlen) {
		pscalenext = 1.5 + qfrandom (1.5);
		dist = (pscale + pscalenext) * 3.0;
		percent = len * origlen;

		particle_new ("pt_smoke", part_tex_smoke, old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  vr_data.realtime + 2.0 - percent * 2.0,
					  12 + (mtwist_rand (&mt) & 3),
					  0.5 + qfrandom (0.125) - percent * 0.40, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorMultAdd (old_origin, len, vec, old_origin);
		pscale = pscalenext;
	}
}

static void
R_GrenadeTrail_QF (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		old_origin, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = vr_data.frametime / maxlen;
	pscale = 6.0 + qfrandom (7.0);

	while (len < maxlen) {
		pscalenext = 6.0 + qfrandom (7.0);
		dist = (pscale + pscalenext) * 2.0;
		percent = len * origlen;

		particle_new ("pt_smoke", part_tex_smoke, old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  vr_data.realtime + 2.0 - percent * 2.0,
					  1 + (mtwist_rand (&mt) & 3),
					  0.625 + qfrandom (0.125) - percent * 0.40, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorMultAdd (old_origin, len, vec, old_origin);
		pscale = pscalenext;
	}
}

static void
R_BloodTrail_QF (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	int			j;
	vec3_t		old_origin, porg, pvel, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = vr_data.frametime / maxlen;
	pscale = 5.0 + qfrandom (10.0);

	while (len < maxlen) {
		pscalenext = 5.0 + qfrandom (10.0);
		dist = (pscale + pscalenext) * 1.5;

		for (j = 0; j < 3; j++) {
			pvel[j] = qfrandom (24.0) - 12.0;
			porg[j] = old_origin[j] + qfrandom (3.0) - 1.5;
		}

		percent = len * origlen;
		pvel[2] -= percent * 40.0;

		particle_new ("pt_grav", part_tex_smoke, porg, pscale, pvel,
					  vr_data.realtime + 2.0 - percent * 2.0,
					  68 + (mtwist_rand (&mt) & 3), 1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorMultAdd (old_origin, len, vec, old_origin);
		pscale = pscalenext;
	}
}

static void
R_SlightBloodTrail_QF (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	int			j;
	vec3_t      old_origin, porg, pvel, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = vr_data.frametime / maxlen;
	pscale = 1.5 + qfrandom (7.5);

	while (len < maxlen) {
		pscalenext = 1.5 + qfrandom (7.5);
		dist = (pscale + pscalenext) * 1.5;

		for (j = 0; j < 3; j++) {
			pvel[j] = (qfrandom (12.0) - 6.0);
			porg[j] = old_origin[j] + qfrandom (3.0) - 1.5;
		}

		percent = len * origlen;
		pvel[2] -= percent * 40;

		particle_new ("pt_grav", part_tex_smoke, porg, pscale, pvel,
					  vr_data.realtime + 1.5 - percent * 1.5,
					  68 + (mtwist_rand (&mt) & 3), 0.75, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorMultAdd (old_origin, len, vec, old_origin);
		pscale = pscalenext;
	}
}

static void
R_WizTrail_QF (entity_t *ent)
{
	float		maxlen, origlen, percent;
	float		dist = 3.0, len = 0.0;
	static int	tracercount;
	vec3_t		old_origin, pvel, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = vr_data.frametime / maxlen;
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		percent = len * origlen;

		tracercount++;
		if (tracercount & 1) {
			pvel[0] = 30.0 * vec[1];
			pvel[1] = 30.0 * -vec[0];
		} else {
			pvel[0] = 30.0 * -vec[1];
			pvel[1] = 30.0 * vec[0];
		}
		pvel[2] = 0.0;

		particle_new ("pt_flame", part_tex_smoke, old_origin,
					  2.0 + qfrandom (1.0) - percent * 2.0, pvel,
					  vr_data.realtime + 0.5 - percent * 0.5,
					  52 + (mtwist_rand (&mt) & 4), 1.0 - percent * 0.125, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_FlameTrail_QF (entity_t *ent)
{
	float		maxlen, origlen, percent;
	float		dist = 3.0, len = 0.0;
	static int	tracercount;
	vec3_t		old_origin, pvel, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = vr_data.frametime / maxlen;
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		percent = len * origlen;

		tracercount++;
		if (tracercount & 1) {
			pvel[0] = 30.0 * vec[1];
			pvel[1] = 30.0 * -vec[0];
		} else {
			pvel[0] = 30.0 * -vec[1];
			pvel[1] = 30.0 * vec[0];
		}
		pvel[2] = 0.0;

		particle_new ("pt_flame", part_tex_smoke, old_origin,
					  2.0 + qfrandom (1.0) - percent * 2.0, pvel,
					  vr_data.realtime + 0.5 - percent * 0.5, 234,
					  1.0 - percent * 0.125, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_VoorTrail_QF (entity_t *ent)
{
	float		maxlen, origlen, percent;
	float		dist = 3.0, len = 0.0;
	int			j;
	vec3_t		subtract, old_origin, porg, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = vr_data.frametime / maxlen;
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		percent = len * origlen;

		for (j = 0; j < 3; j++)
			porg[j] = old_origin[j] + qfrandom (16.0) - 8.0;

		particle_new ("pt_static", part_tex_dot, porg, 1.0 + qfrandom (1.0),
					  vec3_origin, vr_data.realtime + 0.3 - percent * 0.3,
					  9 * 16 + 8 + (mtwist_rand (&mt) & 3), 1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_GlowTrail_QF (entity_t *ent, int glow_color)
{
	float		maxlen, origlen, percent;
	float		dist = 3.0, len = 0.0;
	int			rnd;
	vec3_t		old_origin, org, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = vr_data.frametime / maxlen;
	VectorScale (vec, (maxlen - dist), subtract);

	while (len < maxlen) {
		percent = len * origlen;

		rnd = mtwist_rand (&mt);
		org[0] = old_origin[0] + ((rnd >> 12) & 7) * (5.0/7.0) - 2.5;
		org[1] = old_origin[1] + ((rnd >> 9) & 7) * (5.0/7.0) - 2.5;
		org[2] = old_origin[2] + ((rnd >> 6) & 7) * (5.0/7.0) - 2.5;

		particle_new ("pt_smoke", part_tex_dot, org, 1.0, vec3_origin,
					  vr_data.realtime + 2.0 - percent * 0.2, glow_color,
					  1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_ParticleExplosion_EE (const vec3_t org)
{
/*
	R_NewExplosion (org);
*/
	if (numparticles >= r_maxparticles)
		return;
	particle_new_random ("pt_smokecloud", part_tex_smoke, org, 4, 30, 8,
						 vr_data.realtime + 5.0, mtwist_rand (&mt) & 255,
						 0.5 + qfrandom (0.25), 0.0);
}

static void
R_TeleportSplash_EE (const vec3_t org)
{
	float       vel;
	int         rnd, i, j, k;
	int			l = 896;
	vec3_t      dir, porg, pvel;

	if (numparticles + l >= r_maxparticles) {
		return;
	} // else if (numparticles + l >= r_maxparticles) {
//		l = r_maxparticles - numparticles;
//	}

	for (k = -24; k < 32; k += 4) {
		dir[2] = k * 8;
		for (i = -16; i < 16; i += 4) {
			dir[1] = i * 8;
			for (j = -16; j < 16; j += 4) {
				dir[0] = j * 8;

				rnd = mtwist_rand (&mt);
				porg[0] = org[0] + i + (rnd & 3);
				porg[1] = org[1] + j + ((rnd >> 2) & 3);
				porg[2] = org[2] + k + ((rnd >> 4) & 3);

				VectorNormalize (dir);
				vel = 50 + ((rnd >> 6) & 63);
				VectorScale (dir, vel, pvel);
				particle_new ("pt_grav", part_tex_spark, porg, 0.6, pvel,
							  (vr_data.realtime + 0.2 + (mtwist_rand (&mt) & 15) * 0.01),
							  qfrandom (1.0), 1.0, 0.0);
			}
		}
	}
}

static void
R_RocketTrail_EE (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		old_origin, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = vr_data.frametime / maxlen;
	pscale = 1.5 + qfrandom (1.5);

	while (len < maxlen) {
		pscalenext = 1.5 + qfrandom (1.5);
		dist = (pscale + pscalenext) * 3.0;
		percent = len * origlen;

		particle_new ("pt_smoke", part_tex_smoke, old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  vr_data.realtime + 2.0 - percent * 2.0,
					  mtwist_rand (&mt) & 255,
					  0.5 + qfrandom (0.125) - percent * 0.40, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorScale (vec, len, subtract);
		VectorAdd (old_origin, subtract, old_origin);
		pscale = pscalenext;
	}
}

static void
R_GrenadeTrail_EE (entity_t *ent)
{
	float		dist, maxlen, origlen, percent, pscale, pscalenext;
	float		len = 0.0;
	vec3_t		old_origin, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	origlen = vr_data.frametime / maxlen;
	pscale = 6.0 + qfrandom (7.0);

	while (len < maxlen) {
		pscalenext = 6.0 + qfrandom (7.0);
		dist = (pscale + pscalenext) * 2.0;
		percent = len * origlen;

		particle_new ("pt_smoke", part_tex_smoke, old_origin,
					  pscale + percent * 4.0, vec3_origin,
					  vr_data.realtime + 2.0 - percent * 2.0,
					  mtwist_rand (&mt) & 255,
					  0.625 + qfrandom (0.125) - percent * 0.40, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorScale (vec, len, subtract);
		VectorAdd (old_origin, subtract, old_origin);
		pscale = pscalenext;
	}
}

static void
R_ParticleExplosion_ID (const vec3_t org)
{
	unsigned int	i;
	unsigned int	j = 1024;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j >> 1; i++) {
		particle_new_random ("pt_explode", part_tex_dot, org, 16, 1.0, 256,
							 vr_data.realtime + 5.0, ramp1[0], 1.0, i & 3);
	}
	for (i = 0; i < j / 2; i++) {
		particle_new_random ("pt_explode2", part_tex_dot, org, 16, 1.0, 256,
                             vr_data.realtime + 5.0, ramp1[0], 1.0, i & 3);
	}
}

static void
R_BlobExplosion_ID (const vec3_t org)
{
	unsigned int	i;
	unsigned int	j = 1024;

	if (numparticles >= r_maxparticles)
		return;
	else if (numparticles + j >= r_maxparticles)
		j = r_maxparticles - numparticles;

	for (i = 0; i < j >> 1; i++) {
		particle_new_random ("pt_blob", part_tex_dot, org, 12, 1.0, 256,
							 vr_data.realtime + 1.0 + (mtwist_rand (&mt) & 8) * 0.05,
							 66 + i % 6, 1.0, 0.0);
	}
	for (i = 0; i < j / 2; i++) {
		particle_new_random ("pt_blob2", part_tex_dot, org, 12, 1.0, 256,
							 vr_data.realtime + 1.0 + (mtwist_rand (&mt) & 8) * 0.05,
							 150 + i % 6, 1.0, 0.0);
	}
}

static inline void		// FIXME: inline?
R_RunParticleEffect_ID (const vec3_t org, const vec3_t dir, int color,
						int count)
{
	float			scale;
	int             i;
	vec3_t			porg;

	if (numparticles >= r_maxparticles)
		return;

	if (count > 130) // calculate scale before clipping to particle max
		scale = 3.0;
	else if (count > 20)
		scale = 2.0;
	else
		scale = 1.0;

	if (numparticles + count >= r_maxparticles)
		count = r_maxparticles - numparticles;

	for (i = 0; i < count; i++) {
		int rnd = mtwist_rand (&mt);

		porg[0] = org[0] + scale * (((rnd >> 3) & 15) - 8);
		porg[1] = org[1] + scale * (((rnd >> 7) & 15) - 8);
		porg[2] = org[2] + scale * (((rnd >> 11) & 15) - 8);

		// Note that ParseParticleEffect handles (dir * 15)
		particle_new ("pt_grav", part_tex_dot, porg, 1.0, dir,
					  vr_data.realtime + 0.1 * (i % 5),
					  (color & ~7) + (rnd & 7), 1.0, 0.0);
	}
}

static void
R_BloodPuffEffect_ID (const vec3_t org, int count)
{
	R_RunParticleEffect_ID (org, vec3_origin, 73, count);
}

static void
R_GunshotEffect_ID (const vec3_t org, int count)
{
	R_RunParticleEffect_ID (org, vec3_origin, 0, count);
}

static void
R_LightningBloodEffect_ID (const vec3_t org)
{
	R_RunParticleEffect_ID (org, vec3_origin, 225, 50);
}

static void
R_SpikeEffect_ID (const vec3_t org)
{
	R_RunParticleEffect_ID (org, vec3_origin, 0, 10);
}

static void
R_SuperSpikeEffect_ID (const vec3_t org)
{
	R_RunParticleEffect_ID (org, vec3_origin, 0, 20);
}

static void
R_KnightSpikeEffect_ID (const vec3_t org)
{
	R_RunParticleEffect_ID (org, vec3_origin, 226, 20);
}

static void
R_WizSpikeEffect_ID (const vec3_t org)
{
	R_RunParticleEffect_ID (org, vec3_origin, 20, 30);
}

static void
R_LavaSplash_ID (const vec3_t org)
{
	float       vel;
	int         rnd, i, j;
	int			k = 256;
	vec3_t      dir, porg, pvel;

	if (numparticles + k >= r_maxparticles) {
		return;
	} // else if (numparticles + k >= r_maxparticles) {
//		k = r_maxparticles - numparticles;
//	}

	dir[2] = 256;
	for (i = -128; i < 128; i += 16) {
		for (j = -128; j < 128; j += 16) {
			rnd = mtwist_rand (&mt);
			dir[0] = j + (rnd & 7);
			dir[1] = i + ((rnd >> 6) & 7);

			porg[0] = org[0] + dir[0];
			porg[1] = org[1] + dir[1];
			porg[2] = org[2] + ((rnd >> 9) & 63);

			VectorNormalize (dir);
			rnd = mtwist_rand (&mt);
			vel = 50 + (rnd & 63);
			VectorScale (dir, vel, pvel);
			particle_new ("pt_grav", part_tex_dot, porg, 1.0, pvel,
						  vr_data.realtime + 2 + ((rnd >> 7) & 31) * 0.02,
						  224 + ((rnd >> 12) & 7), 1.0, 0.0);
		}
	}
}

static void
R_TeleportSplash_ID (const vec3_t org)
{
	float       vel;
	int         rnd, i, j, k;
	int			l = 896;
	vec3_t      dir, pdir, porg, pvel;

	if (numparticles + l >= r_maxparticles) {
		return;
	} // else if (numparticles + l >= r_maxparticles) {
//		l = r_maxparticles - numparticles;
//	}

	for (k = -24; k < 32; k += 4) {
		dir[2] = k * 8;
		for (i = -16; i < 16; i += 4) {
			dir[1] = i * 8;
			for (j = -16; j < 16; j += 4) {
				dir[0] = j * 8;

				VectorCopy (dir, pdir);
				VectorNormalize (pdir);

				rnd = mtwist_rand (&mt);
				porg[0] = org[0] + i + (rnd & 3);
				porg[1] = org[1] + j + ((rnd >> 2) & 3);
				porg[2] = org[2] + k + ((rnd >> 4) & 3);

				vel = 50 + ((rnd >> 6) & 63);
				VectorScale (pdir, vel, pvel);
				particle_new ("pt_grav", part_tex_dot, porg, 1.0, pvel,
							  (vr_data.realtime + 0.2 + (mtwist_rand (&mt) & 7) * 0.02),
							  (7 + ((rnd >> 12) & 7)), 1.0, 0.0);
			}
		}
	}
}

static void
R_DarkFieldParticles_ID (entity_t *ent)
{
	int				i, j, k, l = 64;
	unsigned int	rnd;
	float			vel;
	vec3_t			dir, org, porg, pvel;

	if (numparticles + l >= r_maxparticles) {
		return;
	} // else if (numparticles + l >= r_maxparticles) {
//		l = r_maxparticles - numparticles;
//	}

	VectorCopy (ent->origin, org);

	for (i = -16; i < 16; i += 8) {
		dir [1] = i * 8;
		for (j = -16; j < 16; j += 8) {
			dir [0] = j * 8;
			for (k = 0; k < 32; k += 8) {
				dir [2] = k * 8;
				rnd = mtwist_rand (&mt);

				porg[0] = org[0] + i + ((rnd >> 3) & 3);
				porg[1] = org[1] + j + ((rnd >> 5) & 3);
				porg[2] = org[2] + k + ((rnd >> 7) & 3);

				VectorNormalize (dir);
				vel = 50 + ((rnd >> 9) & 63);
				VectorScale (dir, vel, pvel);
				particle_new ("pt_slowgrav", part_tex_dot, porg, 1.5, pvel,
							  (vr_data.realtime + 0.2 + (rnd & 7) * 0.02),
							  (150 + mtwist_rand (&mt) % 6), 1.0, 0.0);
            }
		}
	}
}

static vec3_t		avelocities[NUMVERTEXNORMALS];

static void
R_EntityParticles_ID (entity_t *ent)
{
	int         i, j = NUMVERTEXNORMALS;
	float       angle, sp, sy, cp, cy; // cr, sr
	float       beamlength = 16.0, dist = 64.0;
	vec3_t      forward, porg;

	if (numparticles + j >= r_maxparticles) {
		return;
	} else if (numparticles + j >= r_maxparticles) {
		j = r_maxparticles - numparticles;
	}

	for (i = 0; i < NUMVERTEXNORMALS; i++) {
		int         k;
		for (k = 0; k < 3; k++) {
			avelocities[i][k] = (mtwist_rand (&mt) & 255) * 0.01;
		}
	}

	for (i = 0; i < j; i++) {
		angle = vr_data.realtime * avelocities[i][0];
		cy = cos (angle);
		sy = sin (angle);
		angle = vr_data.realtime * avelocities[i][1];
		cp = cos (angle);
		sp = sin (angle);
// Next 3 lines results aren't currently used, may be in future. --Despair
//		angle = vr_data.realtime * avelocities[i][2];
//		sr = sin (angle);
//		cr = cos (angle);

		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;

		porg[0] = ent->origin[0] + vertex_normals[i][0] * dist +
			forward[0] * beamlength;
		porg[1] = ent->origin[1] + vertex_normals[i][1] * dist +
			forward[1] * beamlength;
		porg[2] = ent->origin[2] + vertex_normals[i][2] * dist +
			forward[2] * beamlength;
		particle_new ("pt_explode", part_tex_dot, porg, 1.0, vec3_origin,
					  vr_data.realtime + 0.01, 0x6f, 1.0, 0);
	}
}

static void
R_RocketTrail_ID (entity_t *ent)
{
	float		maxlen;
	float		dist = 3.0, len = 0.0;
	int			ramp, rnd;
	vec3_t		old_origin, org, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, (maxlen - dist), subtract);

	while (len < maxlen) {
		rnd = mtwist_rand (&mt);
		org[0] = old_origin[0] + ((rnd >> 12) & 7) * (5.0/7.0) - 2.5;
		org[1] = old_origin[1] + ((rnd >> 9) & 7) * (5.0/7.0) - 2.5;
		org[2] = old_origin[2] + ((rnd >> 6) & 7) * (5.0/7.0) - 2.5;
		ramp = rnd & 3;

		particle_new ("pt_fire", part_tex_dot, org, 1.0, vec3_origin,
					  vr_data.realtime + 2.0, ramp3[ramp], 1.0, ramp);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_GrenadeTrail_ID (entity_t *ent)
{
	float			maxlen;
	float			dist = 3.0, len = 0.0;
	unsigned int	ramp, rnd;
	vec3_t			old_origin, org, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, ent->old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		rnd = mtwist_rand (&mt);
		org[0] = old_origin[0] + ((rnd >> 12) & 7) * (5.0/7.0) - 2.5;
		org[1] = old_origin[1] + ((rnd >> 9) & 7) * (5.0/7.0) - 2.5;
		org[2] = old_origin[2] + ((rnd >> 6) & 7) * (5.0/7.0) - 2.5;
		ramp = (rnd & 3) + 2;

		particle_new ("pt_fire", part_tex_dot, org, 1.0, vec3_origin,
					  vr_data.realtime + 2.0, ramp3[ramp], 1.0, ramp);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_BloodTrail_ID (entity_t *ent)
{
	float			maxlen;
	float			dist = 3.0, len = 0.0;
	unsigned int	rnd;
	vec3_t			old_origin, subtract, vec, porg;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		rnd = mtwist_rand (&mt);
		porg[0] = old_origin[0] + ((rnd >> 12) & 7) * (5.0/7.0) - 2.5;
		porg[1] = old_origin[1] + ((rnd >> 9) & 7) * (5.0/7.0) - 2.5;
		porg[2] = old_origin[2] + ((rnd >> 6) & 7) * (5.0/7.0) - 2.5;

		particle_new ("pt_grav", part_tex_dot, porg, 1.0, vec3_origin,
					  vr_data.realtime + 2.0, 67 + (rnd & 3), 1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_SlightBloodTrail_ID (entity_t *ent)
{
	float			maxlen;
	float			dist = 6.0, len = 0.0;
	unsigned int	rnd;
	vec3_t			old_origin, porg, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		rnd = mtwist_rand (&mt);
		porg[0] = old_origin[0] + ((rnd >> 12) & 7) * (5.0/7.0) - 2.5;
		porg[1] = old_origin[1] + ((rnd >> 9) & 7) * (5.0/7.0) - 2.5;
		porg[2] = old_origin[2] + ((rnd >> 6) & 7) * (5.0/7.0) - 2.5;

		particle_new ("pt_grav", part_tex_dot, porg, 1.0, vec3_origin,
					  vr_data.realtime + 1.5, 67 + (rnd & 3), 1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_WizTrail_ID (entity_t *ent)
{
	float		maxlen;
	float		dist = 3.0, len = 0.0;
	static int	tracercount;
	vec3_t		old_origin, pvel, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		tracercount++;
		if (tracercount & 1) {
			pvel[0] = 30.0 * vec[1];
			pvel[1] = 30.0 * -vec[0];
		} else {
			pvel[0] = 30.0 * -vec[1];
			pvel[1] = 30.0 * vec[0];
		}
		pvel[2] = 0.0;

		particle_new ("pt_static", part_tex_dot, old_origin, 1.0, pvel,
					  vr_data.realtime + 0.5, 52 + ((tracercount & 4) << 1),
					  1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_FlameTrail_ID (entity_t *ent)
{
	float		maxlen;
	float		dist = 3.0, len = 0.0;
	static int	tracercount;
	vec3_t		old_origin, pvel, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		tracercount++;
		if (tracercount & 1) {
			pvel[0] = 30.0 * vec[1];
			pvel[1] = 30.0 * -vec[0];
		} else {
			pvel[0] = 30.0 * -vec[1];
			pvel[1] = 30.0 * vec[0];
		}
		pvel[2] = 0.0;

		particle_new ("pt_static", part_tex_dot, old_origin, 1.0, pvel,
					  vr_data.realtime + 0.5, 230 + ((tracercount & 4) << 1),
					  1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static void
R_VoorTrail_ID (entity_t *ent)
{
	float			maxlen;
	float			dist = 3.0, len = 0.0;
	unsigned int	rnd;
	vec3_t			old_origin, porg, subtract, vec;

	if (numparticles >= r_maxparticles)
		return;

	VectorCopy (ent->old_origin, old_origin);
	VectorSubtract (ent->origin, old_origin, vec);
	maxlen = VectorNormalize (vec);
	VectorScale (vec, maxlen - dist, subtract);

	while (len < maxlen) {
		rnd = mtwist_rand (&mt);
		porg[0] = old_origin[0] + ((rnd >> 3) & 15) - 7.5;
		porg[1] = old_origin[1] + ((rnd >> 7) & 15) - 7.5;
		porg[2] = old_origin[2] + ((rnd >> 11) & 15) - 7.5;

		particle_new ("pt_static", part_tex_dot, porg, 1.0, vec3_origin,
					  vr_data.realtime + 0.3, 9 * 16 + 8 + (rnd & 3),
					  1.0, 0.0);
		if (numparticles >= r_maxparticles)
			break;
		len += dist;
		VectorAdd (old_origin, subtract, old_origin);
	}
}

static inline void
set_vertex (trailvtx_t *v, const particle_t *point, float w, const vec3_t bary,
			float off)
{
	VectorCopy (point->org, v->vertex);
	VectorCopy (bary, v->bary);
	v->vertex[3] = w;
	v->texoff = off;
}

static void
count_points (int *num_verts, int *num_elements)
{
	trail_t    *trail;

	*num_verts = 0;
	*num_elements = 0;
	for (trail = trails_active; trail; trail = trail->next) {
		if (trail->num_points < 2)
			continue;
		// +2 for an extra point at either end of the strip
		*num_verts += (trail->num_points + 2) * 2;
		*num_elements += trail->num_points * 2;
	}
}

static void
build_verts (trailvtx_t *v, GLushort *e)
{
	trail_t    *trail;
	int         index = 0;
	particle_t *point, *last_point = 0;
	float       bary[] = {0, 0, 1, 0, 0, 1, 0, 0};
	int         bind = 0;

	for (trail = trails_active; trail; trail = trail->next) {
		if (trail->num_points < 2)
			continue;
		point = trail->points;
		set_vertex (v++, point, -1, bary + bind++, index/2);
		set_vertex (v++, point, 1, bary + bind++, index/2);
		bind %= 3;
		for (; point; point = point->next) {
			set_vertex (v++, point, -1, bary + bind++, index/2 + 1);
			set_vertex (v++, point, 1, bary + bind++, index/2 + 1);
			bind %= 3;
			*e++ = index++;
			*e++ = index++;
			last_point = point;
			R_RunParticlePhysics (point);
		}
		set_vertex (v++, last_point, -1, bary + bind++, index/2 + 1);
		set_vertex (v++, last_point, 1, bary + bind++, index/2 + 1);
		index += 4;
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
	int         num_elements;
	trailvtx_t *verts;
	GLushort   *elements;

	count_points (&num_verts, &num_elements);
	if (!num_elements)
		return;

	verts = alloca (num_verts * sizeof (trailvtx_t));
	elements = alloca (num_verts * sizeof (GLushort));
	build_verts (verts, elements);

	qfeglUseProgram (trail.program);
	qfeglEnableVertexAttribArray (trail.last.location);
	qfeglEnableVertexAttribArray (trail.current.location);
	qfeglEnableVertexAttribArray (trail.next.location);
	if (trail.barycentric.location >= 0)
		qfeglEnableVertexAttribArray (trail.barycentric.location);
	qfeglEnableVertexAttribArray (trail.texoff.location);

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

	for (trl = trails_active; trl; trl = trl->next) {
		int         count;
		if (trl->num_points < 2)
			continue;
		count = trl->num_points * 2;
		qfeglDrawElements (GL_TRIANGLE_STRIP, count, GL_UNSIGNED_SHORT,
						   elements);
		elements += count;
	}

	qfeglDisableVertexAttribArray (trail.last.location);
	qfeglDisableVertexAttribArray (trail.current.location);
	qfeglDisableVertexAttribArray (trail.next.location);
	if (trail.barycentric.location >= 0)
		qfeglDisableVertexAttribArray (trail.barycentric.location);
	qfeglDisableVertexAttribArray (trail.texoff.location);

	expire_trails ();
}

static void
draw_qf_particles (void)
{
	byte       *at;
	int         activeparticles, maxparticle, j, vacount;
	unsigned    k;
	float       minparticledist, scale;
	particle_t *part;
	vec3_t      up_scale, right_scale, up_right_scale, down_right_scale;
	partvert_t *VA;
	mat4_t      vp_mat;
	quat_t      fog;

	Mat4Mult (glsl_projection, glsl_view, vp_mat);

	qfeglDepthMask (GL_FALSE);
	qfeglUseProgram (quake_part.program);
	qfeglEnableVertexAttribArray (quake_part.vertex.location);
	qfeglEnableVertexAttribArray (quake_part.color.location);
	qfeglEnableVertexAttribArray (quake_part.st.location);

	VectorCopy (glsl_Fog_GetColor (), fog);
	fog[3] = glsl_Fog_GetDensity () / 64.0;
	qfeglUniform4fv (quake_part.fog.location, 1, fog);

	qfeglUniformMatrix4fv (quake_part.mvp_matrix.location, 1, false, vp_mat);

	qfeglUniform1i (quake_part.texture.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, part_tex);

	// LordHavoc: particles should not affect zbuffer
	qfeglDepthMask (GL_FALSE);

	minparticledist = DotProduct (r_refdef.vieworg, vpn) +
		r_particles_nearclip->value;

	activeparticles = 0;
	vacount = 0;
	VA = particleVertexArray;
	maxparticle = -1;
	j = 0;

	for (k = 0, part = particles; k < numparticles; k++, part++) {
		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (part->org, vpn) < minparticledist)) {
			at = (byte *) &d_8to24table[(byte) part->color];
			VA[0].color[0] = at[0];
			VA[0].color[1] = at[1];
			VA[0].color[2] = at[2];
			VA[0].color[3] = part->alpha * 255;
			memcpy (VA[1].color, VA[0].color, sizeof (VA[0].color));
			memcpy (VA[2].color, VA[0].color, sizeof (VA[0].color));
			memcpy (VA[3].color, VA[0].color, sizeof (VA[0].color));

			switch (part->tex) {
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

			scale = part->scale;

			VectorScale (vup, scale, up_scale);
			VectorScale (vright, scale, right_scale);

			VectorAdd (right_scale, up_scale, up_right_scale);
			VectorSubtract (right_scale, up_scale, down_right_scale);

			VectorAdd (part->org, down_right_scale, VA[0].vertex);
			VectorSubtract (part->org, up_right_scale, VA[1].vertex);
			VectorSubtract (part->org, down_right_scale, VA[2].vertex);
			VectorAdd (part->org, up_right_scale, VA[3].vertex);

			VA += 4;
			vacount += 6;
		}

		R_RunParticlePhysics (part);

		// LordHavoc: immediate removal of unnecessary particles (must be done
		// to ensure compactor below operates properly in all cases)
		if (part->die < vr_data.realtime) {
			freeparticles[j++] = part;
		} else {
			maxparticle = k;
			activeparticles++;
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

	k = 0;
	while (maxparticle >= activeparticles) {
		*freeparticles[k++] = particles[maxparticle--];
		while (maxparticle >= activeparticles &&
			   particles[maxparticle].die <= vr_data.realtime)
			maxparticle--;
	}
	numparticles = activeparticles;

	qfeglDepthMask (GL_TRUE);
	qfeglDisableVertexAttribArray (quake_part.vertex.location);
	qfeglDisableVertexAttribArray (quake_part.color.location);
	qfeglDisableVertexAttribArray (quake_part.st.location);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
}

static void
draw_id_particles (void)
{
	int         activeparticles, maxparticle, j, vacount;
	unsigned    k;
	float       minparticledist;
	particle_t *part;
	partvert_t *VA;
	mat4_t      vp_mat;
	quat_t      fog;

	Mat4Mult (glsl_projection, glsl_view, vp_mat);

	// LordHavoc: particles should not affect zbuffer
	qfeglDepthMask (GL_FALSE);
	qfeglUseProgram (quake_point.program);
	qfeglEnableVertexAttribArray (quake_point.vertex.location);
	qfeglEnableVertexAttribArray (quake_point.color.location);

	qfeglUniformMatrix4fv (quake_point.mvp_matrix.location, 1, false, vp_mat);

	VectorCopy (glsl_Fog_GetColor (), fog);
	fog[3] = glsl_Fog_GetDensity () / 64.0;
	qfeglUniform4fv (quake_point.fog.location, 1, fog);

	qfeglUniform1i (quake_point.palette.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, glsl_palette);

	minparticledist = DotProduct (r_refdef.vieworg, vpn) +
		r_particles_nearclip->value;

	activeparticles = 0;
	vacount = 0;
	VA = particleVertexArray;
	maxparticle = -1;
	j = 0;

	for (k = 0, part = particles; k < numparticles; k++, part++) {
		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (part->org, vpn) < minparticledist)) {
			VA[0].color[0] = (byte) part->color;
			VectorCopy (part->org, VA[0].vertex);
			VA++;
			vacount++;
		}

		R_RunParticlePhysics (part);

		// LordHavoc: immediate removal of unnecessary particles (must be done
		// to ensure compactor below operates properly in all cases)
		if (part->die < vr_data.realtime) {
			freeparticles[j++] = part;
		} else {
			maxparticle = k;
			activeparticles++;
		}
	}
	qfeglVertexAttribPointer (quake_point.vertex.location, 3, GL_FLOAT,
							 0, sizeof (partvert_t),
							 &particleVertexArray[0].vertex);
	qfeglVertexAttribPointer (quake_point.color.location, 1, GL_UNSIGNED_BYTE,
							 1, sizeof (partvert_t),
							 &particleVertexArray[0].color);
	qfeglDrawArrays (GL_POINTS, 0, vacount);

	k = 0;
	while (maxparticle >= activeparticles) {
		*freeparticles[k++] = particles[maxparticle--];
		while (maxparticle >= activeparticles &&
			   particles[maxparticle].die <= vr_data.realtime)
			maxparticle--;
	}
	numparticles = activeparticles;

	qfeglDepthMask (GL_TRUE);
	qfeglDisableVertexAttribArray (quake_point.vertex.location);
	qfeglDisableVertexAttribArray (quake_point.color.location);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglDisable (GL_TEXTURE_2D);
}

void
glsl_R_DrawParticles (void)
{
	draw_trails ();
	if (!r_particles->int_val || !numparticles)
		return;
	if (r_particles_style->int_val) {
		draw_qf_particles ();
	} else {
		draw_id_particles ();
	}
}

static vid_particle_funcs_t particles_trail = {
	R_RocketTrail_trail,
	R_GrenadeTrail_QF,
	R_BloodTrail_QF,
	R_SlightBloodTrail_QF,
	R_WizTrail_QF,
	R_FlameTrail_QF,
	R_VoorTrail_QF,
	R_GlowTrail_QF,
	R_RunParticleEffect_QF,
	R_BloodPuffEffect_QF,
	R_GunshotEffect_QF,
	R_LightningBloodEffect_QF,
	R_SpikeEffect_QF,
	R_KnightSpikeEffect_QF,
	R_SuperSpikeEffect_QF,
	R_WizSpikeEffect_QF,
	R_BlobExplosion_QF,
	R_ParticleExplosion_QF,
	R_ParticleExplosion2_QF,
	R_LavaSplash_QF,
	R_TeleportSplash_QF,
	R_DarkFieldParticles_ID,
	R_EntityParticles_ID,
	R_Particle_New,
	R_Particle_NewRandom,
};

static vid_particle_funcs_t particles_QF = {
	R_RocketTrail_QF,
	R_GrenadeTrail_QF,
	R_BloodTrail_QF,
	R_SlightBloodTrail_QF,
	R_WizTrail_QF,
	R_FlameTrail_QF,
	R_VoorTrail_QF,
	R_GlowTrail_QF,
	R_RunParticleEffect_QF,
	R_BloodPuffEffect_QF,
	R_GunshotEffect_QF,
	R_LightningBloodEffect_QF,
	R_SpikeEffect_QF,
	R_KnightSpikeEffect_QF,
	R_SuperSpikeEffect_QF,
	R_WizSpikeEffect_QF,
	R_BlobExplosion_QF,
	R_ParticleExplosion_QF,
	R_ParticleExplosion2_QF,
	R_LavaSplash_QF,
	R_TeleportSplash_QF,
	R_DarkFieldParticles_ID,
	R_EntityParticles_ID,
	R_Particle_New,
	R_Particle_NewRandom,
};

static vid_particle_funcs_t particles_ID = {
	R_RocketTrail_ID,
	R_GrenadeTrail_ID,
	R_BloodTrail_ID,
	R_SlightBloodTrail_ID,
	R_WizTrail_ID,
	R_FlameTrail_ID,
	R_VoorTrail_ID,
	R_GlowTrail_QF,
	R_RunParticleEffect_ID,
	R_BloodPuffEffect_ID,
	R_GunshotEffect_ID,
	R_LightningBloodEffect_ID,
	R_SpikeEffect_ID,
	R_KnightSpikeEffect_ID,
	R_SuperSpikeEffect_ID,
	R_WizSpikeEffect_ID,
	R_BlobExplosion_ID,
	R_ParticleExplosion_ID,
	R_ParticleExplosion2_QF,
	R_LavaSplash_ID,
	R_TeleportSplash_ID,
	R_DarkFieldParticles_ID,
	R_EntityParticles_ID,
	R_Particle_New,
	R_Particle_NewRandom,
};

static vid_particle_funcs_t particles_QF_egg = {
	R_RocketTrail_EE,
	R_GrenadeTrail_EE,
	R_BloodTrail_QF,
	R_SlightBloodTrail_QF,
	R_WizTrail_QF,
	R_FlameTrail_QF,
	R_VoorTrail_QF,
	R_GlowTrail_QF,
	R_RunParticleEffect_QF,
	R_BloodPuffEffect_QF,
	R_GunshotEffect_QF,
	R_LightningBloodEffect_QF,
	R_SpikeEffect_QF,
	R_KnightSpikeEffect_QF,
	R_SuperSpikeEffect_QF,
	R_WizSpikeEffect_QF,
	R_BlobExplosion_QF,
	R_ParticleExplosion_EE,
	R_ParticleExplosion2_QF,
	R_LavaSplash_QF,
	R_TeleportSplash_EE,
	R_DarkFieldParticles_ID,
	R_EntityParticles_ID,
	R_Particle_New,
	R_Particle_NewRandom,
};

static vid_particle_funcs_t particles_ID_egg = {
	R_RocketTrail_EE,
	R_GrenadeTrail_EE,
	R_BloodTrail_ID,
	R_SlightBloodTrail_ID,
	R_WizTrail_ID,
	R_FlameTrail_ID,
	R_VoorTrail_ID,
	R_GlowTrail_QF,
	R_RunParticleEffect_ID,
	R_BloodPuffEffect_ID,
	R_GunshotEffect_ID,
	R_LightningBloodEffect_ID,
	R_SpikeEffect_ID,
	R_KnightSpikeEffect_ID,
	R_SuperSpikeEffect_ID,
	R_WizSpikeEffect_ID,
	R_BlobExplosion_ID,
	R_ParticleExplosion_EE,
	R_ParticleExplosion2_QF,
	R_LavaSplash_ID,
	R_TeleportSplash_EE,
	R_DarkFieldParticles_ID,
	R_EntityParticles_ID,
	R_Particle_New,
	R_Particle_NewRandom,
};

void
glsl_r_easter_eggs_f (cvar_t *var)
{
	if (easter_eggs) {
		if (easter_eggs->int_val) {
			if (r_particles_style->int_val) {
				glsl_vid_render_funcs.particles = &particles_QF_egg;
			} else {
				glsl_vid_render_funcs.particles = &particles_ID_egg;
			}
		} else if (r_particles_style) {
			if (r_particles_style->int_val > 1) {
				glsl_vid_render_funcs.particles = &particles_QF;
			} else if (r_particles_style->int_val) {
				glsl_vid_render_funcs.particles = &particles_trail;
			} else {
				glsl_vid_render_funcs.particles = &particles_ID;
			}
		}
	}
}

void
glsl_r_particles_style_f (cvar_t *var)
{
	glsl_r_easter_eggs_f (easter_eggs);
}

static void
R_ParticleFunctionInit (void)
{
	glsl_r_particles_style_f (r_particles_style);
	glsl_r_easter_eggs_f (easter_eggs);
}

static void
r_particles_nearclip_f (cvar_t *var)
{
	Cvar_SetValue (r_particles_nearclip, bound (r_nearclip->value, var->value,
												r_farclip->value));
}

static void
r_particles_f (cvar_t *var)
{
	R_MaxParticlesCheck (var, r_particles_max);
}

static void
r_particles_max_f (cvar_t *var)
{
	R_MaxParticlesCheck (r_particles, var);
}

void
glsl_R_Particles_Init_Cvars (void)
{
	easter_eggs = Cvar_Get ("easter_eggs", "0", CVAR_NONE, r_easter_eggs_f,
							"Enables easter eggs.");
	r_particles = Cvar_Get ("r_particles", "1", CVAR_ARCHIVE, r_particles_f,
							"Toggles drawing of particles.");
	r_particles_max = Cvar_Get ("r_particles_max", "2048", CVAR_ARCHIVE,
								r_particles_max_f, "Maximum amount of "
								"particles to display. No maximum, minimum "
								"is 0.");
	r_particles_nearclip = Cvar_Get ("r_particles_nearclip", "32",
									 CVAR_ARCHIVE, r_particles_nearclip_f,
									 "Distance of the particle near clipping "
									 "plane from the player.");
	r_particles_style = Cvar_Get ("r_particles_style", "1", CVAR_ARCHIVE,
								  r_particles_style_f, "Sets particle style. "
								  "0 for Id, 1 for QF.");
	R_ParticleFunctionInit ();
}

void
glsl_R_Particle_New (const char *type, int texnum, const vec3_t org,
					 float scale, const vec3_t vel, float die, int color,
					 float alpha, float ramp)
{
	if (numparticles >= r_maxparticles)
		return;
	particle_new (type, texnum, org, scale, vel, die, color, alpha, ramp);
}

void
glsl_R_Particle_NewRandom (const char *type, int texnum, const vec3_t org,
						   int org_fuzz, float scale, int vel_fuzz, float die,
						   int color, float alpha, float ramp)
{
	if (numparticles >= r_maxparticles)
		return;
	particle_new_random (type, texnum, org, org_fuzz, scale, vel_fuzz, die,
						 color, alpha, ramp);
}
