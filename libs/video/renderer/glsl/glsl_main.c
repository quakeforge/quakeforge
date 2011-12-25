/*
	glsl_main.c

	GLSL rendering

	Copyright (C) 2011 Bill Currie <bill@taniwha.org>

	Author: Bill Currie <bill@taniwha.org>
	Date: 2011/12/23

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
# include "string.h"
#endif
#ifdef HAVE_STRINGS_H
# include "strings.h"
#endif

#include "QF/cvar.h"
#include "QF/image.h"
#include "QF/render.h"
#include "QF/screen.h"
#include "QF/skin.h"

#include "QF/GL/qf_textures.h"

#include "r_cvar.h"
#include "r_dynamic.h"
#include "r_screen.h"

VISIBLE vec3_t r_origin, vpn, vright, vup;
VISIBLE refdef_t r_refdef;
qboolean r_cache_thrash;
int r_init;
int r_framecount;
int d_lightstylevalue[256];
int r_visframecount;
entity_t r_worldentity;

void
r_easter_eggs_f (cvar_t *var)
{
}

void
r_particles_style_f (cvar_t *var)
{
}

void
gl_overbright_f (cvar_t *var)
{
}

VISIBLE void
R_ViewChanged (float aspect)
{
}

VISIBLE void
R_RenderView (void)
{
}

VISIBLE void
R_Init (void)
{
	R_InitParticles ();
}

VISIBLE void
R_Particles_Init_Cvars (void)
{
}

VISIBLE void
R_Particle_New (ptype_t type, int texnum, const vec3_t org, float scale,
			    const vec3_t vel, float die, int color, float alpha, float ramp)
{
}

VISIBLE void
R_Particle_NewRandom (ptype_t type, int texnum, const vec3_t org, int org_fuzz,
					  float scale, int vel_fuzz, float die, int color,
					  float alpha, float ramp)
{
}

VISIBLE void
R_ClearParticles (void)
{
}

VISIBLE void
SCR_UpdateScreen (double realtime, SCR_Func *scr_funcs)
{
}

VISIBLE void
R_NewMap (model_t *worldmodel, struct model_s **models, int num_models)
{
	memset (&r_worldentity, 0, sizeof (r_worldentity));
	r_worldentity.model = worldmodel;

	R_FreeAllEntities ();
	R_ClearParticles ();
}

VISIBLE void
R_LoadSkys (const char *sky)
{
}

VISIBLE void
Fog_Update (float density, float red, float green, float blue, float time)
{
}

VISIBLE void
Fog_ParseWorldspawn (struct plitem_s *worldspawn)
{
}

VISIBLE void
Skin_Player_Model (model_t *model)
{
}

VISIBLE void
Skin_Set_Translate (int top, int bottom, void *_dest)
{
}

VISIBLE void
Skin_Do_Translation (skin_t *player_skin, int slot, skin_t *skin)
{
}

VISIBLE void
Skin_Do_Translation_Model (struct model_s *model, int skinnum, int slot,
						   skin_t *skin)
{
}

VISIBLE void
Skin_Process (skin_t *skin, struct tex_s * tex)
{
}

VISIBLE void
Skin_Init_Translation (void)
{
}

VISIBLE void
R_LineGraph (int x, int y, int *h_vals, int count)
{
}

static void
particle_explosion (const vec3_t org)
{
}

static void
teleport_splash (const vec3_t org)
{
}

static void
rocket_trail (const entity_t *ent)
{
}

static void
grenade_trail (const entity_t *ent)
{
}

static void
blob_explosion (const vec3_t org)
{
}

static void
lava_splash (const vec3_t org)
{
}

static void
blood_puff_effect (const vec3_t org, int count)
{
}

static void
gunshot_effect (const vec3_t org, int count)
{
}

static void
lightnight_blood_effect (const vec3_t org)
{
}

static inline void		// FIXME: inline?
run_particle_effect (const vec3_t org, const vec3_t dir, int color,
					 int count)
{
}

static void
spike_effect (const vec3_t org)
{
}

static void
super_spike_effect (const vec3_t org)
{
}

static void
knight_spike_effect (const vec3_t org)
{
}

static void
wiz_spike_effect (const vec3_t org)
{
}

static void
blood_trail (const entity_t *ent)
{
}

static void
slight_blood_trail (const entity_t *ent)
{
}

static void
wiz_trail (const entity_t *ent)
{
}

static void
flame_trail (const entity_t *ent)
{
}

static void
voor_trail (const entity_t *ent)
{
}

static void
dark_field_particles (const entity_t *ent)
{
}

static void
entity_particles (const entity_t *ent)
{
}

static void
particle_explosion_2 (const vec3_t org, int colorStart, int colorLength)
{
}

static void
glow_trail (const entity_t *ent, int glow_color)
{
}

VISIBLE void
R_InitParticles (void)
{
	R_BlobExplosion = blob_explosion;
	R_LavaSplash = lava_splash;
	R_BloodPuffEffect = blood_puff_effect;
	R_GunshotEffect = gunshot_effect;
	R_LightningBloodEffect = lightnight_blood_effect;
	R_RunParticleEffect = run_particle_effect;
	R_SpikeEffect = spike_effect;
	R_SuperSpikeEffect = super_spike_effect;
	R_KnightSpikeEffect = knight_spike_effect;
	R_WizSpikeEffect = wiz_spike_effect;
	R_BloodTrail = blood_trail;
	R_SlightBloodTrail = slight_blood_trail;
	R_WizTrail = wiz_trail;
	R_FlameTrail = flame_trail;
	R_VoorTrail = voor_trail;
	R_DarkFieldParticles = dark_field_particles;
	R_EntityParticles = entity_particles;
	R_ParticleExplosion2 = particle_explosion_2;
	R_GlowTrail = glow_trail;
	R_ParticleExplosion = particle_explosion;
	R_TeleportSplash = teleport_splash;
	R_RocketTrail = rocket_trail;
	R_GrenadeTrail = grenade_trail;
}

VISIBLE tex_t *
SCR_ScreenShot (int width, int height)
{
	return 0;
}

VISIBLE void
SCR_ScreenShot_f (void)
{
}

VISIBLE int
GL_LoadTexture (const char *identifier, int width, int height, byte *data,
				qboolean mipmap, qboolean alpha, int bytesperpixel)
{
	return 0;
}

VISIBLE void
R_ClearState (void)
{
	R_ClearEfrags ();
	R_ClearDlights ();
	R_ClearParticles ();
}
