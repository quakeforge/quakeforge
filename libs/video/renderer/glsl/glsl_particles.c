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

static void
draw_qf_particles (void)
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

	glsl_Fog_GetColor (fog);
	fog[3] = glsl_Fog_GetDensity () / 64.0;
	qfeglUniform4fv (quake_part.fog.location, 1, fog);

	qfeglUniformMatrix4fv (quake_part.mvp_matrix.location, 1, false,
						   &vp_mat[0][0]);

	qfeglUniform1i (quake_part.texture.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, part_tex);

	// LordHavoc: particles should not affect zbuffer
	qfeglDepthMask (GL_FALSE);

	minparticledist = DotProduct (r_refdef.viewposition, vpn) +
		r_particles_nearclip->value;

	vacount = 0;
	VA = particleVertexArray;

	for (unsigned i = 0; i < numparticles; i++) {
		particle_t *p = &particles[i];
		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (p->pos, vpn) < minparticledist)) {
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

			VectorScale (vup, scale, up_scale);
			VectorScale (vright, scale, right_scale);

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
draw_id_particles (void)
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
						   &vp_mat[0][0]);

	glsl_Fog_GetColor (fog);
	fog[3] = glsl_Fog_GetDensity () / 64.0;
	qfeglUniform4fv (quake_point.fog.location, 1, fog);

	qfeglUniform1i (quake_point.palette.location, 0);
	qfeglActiveTexture (GL_TEXTURE0 + 0);
	qfeglEnable (GL_TEXTURE_2D);
	qfeglBindTexture (GL_TEXTURE_2D, glsl_palette);

	minparticledist = DotProduct (r_refdef.viewposition, vpn) +
		r_particles_nearclip->value;

	vacount = 0;
	VA = particleVertexArray;

	for (unsigned i = 0; i < numparticles; i++) {
		particle_t *p = &particles[i];
		// Don't render particles too close to us.
		// Note, we must still do physics and such on them.
		if (!(DotProduct (p->pos, vpn) < minparticledist)) {
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
glsl_R_DrawParticles (void)
{
	if (!r_particles->int_val || !numparticles)
		return;
	R_RunParticles (vr_data.frametime);
	if (0/*FIXME r_particles_style->int_val*/) {
		draw_qf_particles ();
	} else {
		draw_id_particles ();
	}
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
}
