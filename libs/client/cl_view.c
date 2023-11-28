/*
	cl_view.c

	player eye positioning

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

#include "QF/cmd.h"
#include "QF/cvar.h"
#include "QF/msg.h"
#include "QF/screen.h"

#include "QF/plugin/vid_render.h"
#include "QF/scene/entity.h"
#include "QF/scene/scene.h"
#include "QF/simd/vec4f.h"

#include "compat.h"

#include "client/chase.h"
#include "client/entities.h"
#include "client/hud.h"
#include "client/input.h"
#include "client/view.h"
#include "client/world.h"

/*
	The view is allowed to move slightly from it's true position for bobbing,
	but if it exceeds 8 pixels linear distance (spherical, not box), the list
	of entities sent from the server may not include everything in the pvs,
	especially when crossing a water boudnary.
*/

float scr_ofsx;
static cvar_t scr_ofsx_cvar = {
	.name = "scr_ofsx",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &scr_ofsx },
};
float scr_ofsy;
static cvar_t scr_ofsy_cvar = {
	.name = "scr_ofsy",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &scr_ofsy },
};
float scr_ofsz;
static cvar_t scr_ofsz_cvar = {
	.name = "scr_ofsz",
	.description =
		"None",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &scr_ofsz },
};

float cl_rollspeed;
static cvar_t cl_rollspeed_cvar = {
	.name = "cl_rollspeed",
	.description =
		"How quickly you straighten out after strafing",
	.default_value = "200",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &cl_rollspeed },
};
float cl_rollangle;
static cvar_t cl_rollangle_cvar = {
	.name = "cl_rollangle",
	.description =
		"How much your screen tilts when strafing",
	.default_value = "2.0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &cl_rollangle },
};

float cl_bob;
static cvar_t cl_bob_cvar = {
	.name = "cl_bob",
	.description =
		"How much your weapon moves up and down when walking",
	.default_value = "0.02",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &cl_bob },
};
float cl_bobcycle;
static cvar_t cl_bobcycle_cvar = {
	.name = "cl_bobcycle",
	.description =
		"How quickly your weapon moves up and down when walking",
	.default_value = "0.6",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &cl_bobcycle },
};
float cl_bobup;
static cvar_t cl_bobup_cvar = {
	.name = "cl_bobup",
	.description =
		"How long your weapon stays up before cycling when walking",
	.default_value = "0.5",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &cl_bobup },
};

float v_centermove;
static cvar_t v_centermove_cvar = {
	.name = "v_centermove",
	.description =
		"How far the player must move forward before the view re-centers",
	.default_value = "0.15",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_centermove },
};
float v_centerspeed;
static cvar_t v_centerspeed_cvar = {
	.name = "v_centerspeed",
	.description =
		"How quickly you return to a center view after a lookup or lookdown",
	.default_value = "500",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_centerspeed },
};

float v_kicktime;
static cvar_t v_kicktime_cvar = {
	.name = "v_kicktime",
	.description =
		"How long the kick from an attack lasts",
	.default_value = "0.5",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_kicktime },
};
float v_kickroll;
static cvar_t v_kickroll_cvar = {
	.name = "v_kickroll",
	.description =
		"How much you lean when hit",
	.default_value = "0.6",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_kickroll },
};
float v_kickpitch;
static cvar_t v_kickpitch_cvar = {
	.name = "v_kickpitch",
	.description =
		"How much you look up when hit",
	.default_value = "0.6",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_kickpitch },
};

int cl_cshift_bonus;
static cvar_t cl_cshift_bonus_cvar = {
	.name = "cl_cshift_bonus",
	.description =
		"Show bonus flash on item pickup",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_cshift_bonus },
};
int cl_cshift_contents;
static cvar_t cl_cshift_contents_cvar = {
	.name = "cl_cshift_content",
	.description =
		"Shift view colors for contents (water, slime, etc)",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_cshift_contents },
};
int cl_cshift_damage;
static cvar_t cl_cshift_damage_cvar = {
	.name = "cl_cshift_damage",
	.description =
		"Shift view colors on damage",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_cshift_damage },
};
int cl_cshift_powerup;
static cvar_t cl_cshift_powerup_cvar = {
	.name = "cl_cshift_powerup",
	.description =
		"Shift view colors for powerups",
	.default_value = "1",
	.flags = CVAR_ARCHIVE,
	.value = { .type = &cexpr_int, .value = &cl_cshift_powerup },
};

float v_iyaw_cycle;
static cvar_t v_iyaw_cycle_cvar = {
	.name = "v_iyaw_cycle",
	.description =
		"How far you tilt right and left when v_idlescale is enabled",
	.default_value = "2",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_iyaw_cycle },
};
float v_iroll_cycle;
static cvar_t v_iroll_cycle_cvar = {
	.name = "v_iroll_cycle",
	.description =
		"How quickly you tilt right and left when v_idlescale is enabled",
	.default_value = "0.5",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_iroll_cycle },
};
float v_ipitch_cycle;
static cvar_t v_ipitch_cycle_cvar = {
	.name = "v_ipitch_cycle",
	.description =
		"How quickly you lean forwards and backwards when v_idlescale is "
		"enabled",
	.default_value = "1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_ipitch_cycle },
};
float v_iyaw_level;
static cvar_t v_iyaw_level_cvar = {
	.name = "v_iyaw_level",
	.description =
		"How far you tilt right and left when v_idlescale is enabled",
	.default_value = "0.3",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_iyaw_level },
};
float v_iroll_level;
static cvar_t v_iroll_level_cvar = {
	.name = "v_iroll_level",
	.description =
		"How far you tilt right and left when v_idlescale is enabled",
	.default_value = "0.1",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_iroll_level },
};
float v_ipitch_level;
static cvar_t v_ipitch_level_cvar = {
	.name = "v_ipitch_level",
	.description =
		"How far you lean forwards and backwards when v_idlescale is enabled",
	.default_value = "0.3",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_ipitch_level },
};

float v_idlescale;
static cvar_t v_idlescale_cvar = {
	.name = "v_idlescale",
	.description =
		"Toggles whether the view remains idle",
	.default_value = "0",
	.flags = CVAR_NONE,
	.value = { .type = &cexpr_float, .value = &v_idlescale },
};

float       v_dmg_time, v_dmg_roll, v_dmg_pitch;

vec4f_t     v_idle_yaw;
vec4f_t     v_idle_roll;
vec4f_t     v_idle_pitch;

static cshift_t cshift_empty = { {130,  80, 50},   0};
static cshift_t cshift_water = { {130,  80, 50}, 128};
static cshift_t cshift_slime = { {  0,  25,  5}, 150};
static cshift_t cshift_lava =  { {255,  80,  0}, 150};
static cshift_t cshift_bonus = { {215, 186, 60},  50};

static cshift_t armor_blood[] = {
	{ {255,   0,   0} },	// blood
	{ {220,  50,  50} },	// armor + blood
	{ {200, 100, 100} },	// armor > blood need two for logic
	{ {200, 100, 100} },	// armor > blood need two for logic
};

static cshift_t powerup[] = {
	{ {  0,   0,   0},   0},
	{ {100, 100, 100}, 100},	// IT_INVISIBILITY
	{ {255, 255,   0},  30},	// IT_INVULNERABILITY
	{ {255, 255,   0},  30},	// IT_INVULNERABILITY
	{ {  0, 255,   0},  20},	// IT_SUIT
	{ {  0, 255,   0},  20},	// IT_SUIT
	{ {  0, 255,   0},  20},	// IT_SUIT
	{ {  0, 255,   0},  20},	// IT_SUIT
	{ {  0,   0, 255},  30},	// IT_QUAD
	{ {  0,   0, 255},  30},	// IT_QUAD
	{ {255,   0, 255},  30},	// IT_INVULNERABILITY | IT_QUAD
	{ {255,   0, 255},  30},	// IT_INVULNERABILITY | IT_QUAD
	{ {255,   0, 255},  30},	// IT_INVULNERABILITY | IT_QUAD
	{ {255,   0, 255},  30},	// IT_INVULNERABILITY | IT_QUAD
	{ {255,   0, 255},  30},	// IT_INVULNERABILITY | IT_QUAD
	{ {255,   0, 255},  30},	// IT_INVULNERABILITY | IT_QUAD
};

#define sqr(x) ((x) * (x))

float
V_CalcRoll (const vec3_t angles, vec4f_t velocity)
{
	float       side, sign, value;
	vec3_t      forward, right, up;

	AngleVectors (angles, forward, right, up);
	side = DotProduct (velocity, right);
	sign = side < 0 ? -1 : 1;
	side = fabs (side);

	value = cl_rollangle;

	if (side < cl_rollspeed)
		side = side * value / cl_rollspeed;
	else
		side = value;

	return side * sign;
}

static float
V_CalcBob (viewstate_t *vs)
{
	vec4f_t     velocity = vs->velocity;
	float       cycle;
	static double bobtime;
	static float bob;

	if (!vs->bob_enabled)
		return 0;

	if (vs->onground == -1)
		return bob;						// just use old value

	bobtime += vs->frametime;
	cycle = bobtime - (int) (bobtime / cl_bobcycle) *
		cl_bobcycle;
	cycle /= cl_bobcycle;
	if (cycle < cl_bobup)
		cycle = cycle / cl_bobup;
	else
		cycle = 1 + (cycle - cl_bobup) / (1.0 - cl_bobup);

	// bob is proportional to velocity in the xy plane
	// (don't count Z, or jumping messes it up)
	velocity[2] = 0;
	bob = sqrt (dotf (velocity, velocity)[0]) * cl_bob;
	bob = bob * 0.3 + bob * 0.7 * sin (cycle * M_PI);
	if (bob > 4)
		bob = 4;
	else if (bob < -7)
		bob = -7;
	return bob;
}

void
V_StartPitchDrift (viewstate_t *vs)
{
	if (vs->laststop == vs->time) {
		return;					// something else is keeping it from drifting
	}

	if (vs->nodrift || !vs->pitchvel) {
		vs->pitchvel = v_centerspeed;
		vs->nodrift = false;
		vs->driftmove = 0;
	}
}

static void
V_StartPitchDrift_f (void *data)
{
	V_StartPitchDrift (data);
}

void
V_StopPitchDrift (viewstate_t *vs)
{
	vs->laststop = vs->time;
	vs->nodrift = true;
	vs->pitchvel = 0;
}

/*
	V_DriftPitch

	Moves the client pitch angle towards vs->idealpitch sent by the server.

	If the user is adjusting pitch manually, either with lookup/lookdown,
	mlook and mouse, or klook and keyboard, pitch drifting is constantly
	stopped.

	Drifting is enabled when the center view key is hit, mlook is released
	and lookspring is non 0, or when
*/
static void
V_DriftPitch (viewstate_t *vs)
{
	float       delta, move;
	float       forwardmove = vs->movecmd[0];

	if (noclip_anglehack || vs->onground == -1 || !vs->drift_enabled) {
		vs->driftmove = 0;
		vs->pitchvel = 0;
		return;
	}

	// don't count small mouse motion
	if (vs->nodrift) {
		if (fabs (forwardmove) < cl_forwardspeed)
			vs->driftmove = 0;
		else
			vs->driftmove += vs->frametime;

		if (vs->driftmove > v_centermove) {
			V_StartPitchDrift (vs);
		}
		return;
	}

	delta = vs->idealpitch - vs->player_angles[PITCH];

	if (!delta) {
		vs->pitchvel = 0;
		return;
	}

	move = vs->frametime * vs->pitchvel;
	vs->pitchvel += vs->frametime * v_centerspeed;

	if (delta > 0) {
		if (move > delta) {
			vs->pitchvel = 0;
			move = delta;
		}
		vs->player_angles[PITCH] += move;
	} else if (delta < 0) {
		if (move > -delta) {
			vs->pitchvel = 0;
			move = -delta;
		}
		vs->player_angles[PITCH] -= move;
	}
}

/* PALETTE FLASHES */

void
V_ParseDamage (qmsg_t *net_message, viewstate_t *vs)
{
	float       count, side;
	int         armor, blood;
	vec4f_t     origin = vs->player_origin;
	vec_t      *angles = vs->player_angles;
	vec3_t      from, forward, right, up;

	armor = MSG_ReadByte (net_message);
	blood = MSG_ReadByte (net_message);
	MSG_ReadCoordV (net_message, from);

	count = blood * 0.5 + armor * 0.5;
	if (count < 10)
		count = 10;

	if (cl_cshift_damage
		|| (vs->force_cshifts & INFO_CSHIFT_DAMAGE)) {
		cshift_t   *cshift = &vs->cshifts[CSHIFT_DAMAGE];
		int         percent = cshift->percent;
		*cshift = armor_blood[(2 * (armor > blood)) + (armor > 0)];
		cshift->percent = percent + 3 * count;
		cshift->percent = bound (0, cshift->percent, 150);
		cshift->initialpct = cshift->percent;
		cshift->time = vs->time;
	}

	// calculate view angle kicks
	VectorSubtract (from, origin, from);
	VectorNormalize (from);

	AngleVectors (angles, forward, right, up);

	side = DotProduct (from, right);
	v_dmg_roll = count * side * v_kickroll;

	side = DotProduct (from, forward);
	v_dmg_pitch = count * side * v_kickpitch;

	v_dmg_time = v_kicktime;
}

static void
V_cshift_f (void)
{
	cshift_empty.destcolor[0] = atoi (Cmd_Argv (1));
	cshift_empty.destcolor[1] = atoi (Cmd_Argv (2));
	cshift_empty.destcolor[2] = atoi (Cmd_Argv (3));
	cshift_empty.percent = atoi (Cmd_Argv (4));
}

/*
	V_BonusFlash_f

	When you run over an item, the server sends this command
*/
static void
V_BonusFlash_f (void *data)
{
	viewstate_t *vs = data;
	if (!cl_cshift_bonus
		&& !(vs->force_cshifts & INFO_CSHIFT_BONUS))
		return;

	vs->cshifts[CSHIFT_BONUS] = cshift_bonus;
	vs->cshifts[CSHIFT_BONUS].initialpct = vs->cshifts[CSHIFT_BONUS].percent;
	vs->cshifts[CSHIFT_BONUS].time = vs->time;
}

/*
	V_SetContentsColor

	Underwater, lava, etc each has a color shift
*/
void
V_SetContentsColor (viewstate_t *vs, int contents)
{
	if (!cl_cshift_contents
		&& !(vs->force_cshifts & INFO_CSHIFT_CONTENTS)) {
		vs->cshifts[CSHIFT_CONTENTS] = cshift_empty;
		return;
	}

	switch (contents) {
		case CONTENTS_EMPTY:
			vs->cshifts[CSHIFT_CONTENTS] = cshift_empty;
			break;
		case CONTENTS_LAVA:
			vs->cshifts[CSHIFT_CONTENTS] = cshift_lava;
			break;
		case CONTENTS_SOLID:
		case CONTENTS_SLIME:
			vs->cshifts[CSHIFT_CONTENTS] = cshift_slime;
			break;
		default:
			vs->cshifts[CSHIFT_CONTENTS] = cshift_water;
	}
}

static void
V_CalcPowerupCshift (viewstate_t *vs)
{
	vs->cshifts[CSHIFT_POWERUP] = powerup[vs->powerup_index];
}

/*
  V_CalcBlend

  LordHavoc made this a real, true alpha blend.  Cleaned it up
  a bit, but otherwise this is his code.  --KB
*/
static void
V_CalcBlend (viewstate_t *vs)
{
	float       a2, a3;
	float       r = 0, g = 0, b = 0, a = 0;
	int         i;

	for (i = 0; i < NUM_CSHIFTS; i++) {
		a2 = vs->cshifts[i].percent / 255.0;

		if (!a2)
			continue;

		a2 = min (a2, 1.0);
		r += (vs->cshifts[i].destcolor[0] - r) * a2;
		g += (vs->cshifts[i].destcolor[1] - g) * a2;
		b += (vs->cshifts[i].destcolor[2] - b) * a2;

		a3 = (1.0 - a) * (1.0 - a2);
		a = 1.0 - a3;
	}

	// LordHavoc: saturate color
	if (a) {
		a2 = 1.0 / a;
		r *= a2;
		g *= a2;
		b *= a2;
	}

	vs->cshift_color[0] = min (r, 255.0) / 255.0;
	vs->cshift_color[1] = min (g, 255.0) / 255.0;
	vs->cshift_color[2] = min (b, 255.0) / 255.0;
	vs->cshift_color[3] = bound (0.0, a, 1.0);
}

static void
V_DropCShift (cshift_t *cs, double time, float droprate)
{
	if (cs->time < 0) {
		  cs->percent = 0;
	} else {
		cs->percent = cs->initialpct - (time - cs->time) * droprate;
		if (cs->percent  <= 0) {
			cs->percent = 0;
			cs->time = -1;
		}
	}
}

void
V_PrepBlend (viewstate_t *vs)
{
	qfZoneNamed (zone, true);
	int         i, j;

	if (cl_cshift_powerup
		|| (vs->force_cshifts & INFO_CSHIFT_POWERUP))
		V_CalcPowerupCshift (vs);

	bool        cshift_changed = false;

	for (i = 0; i < NUM_CSHIFTS; i++) {
		if (vs->cshifts[i].percent != vs->prev_cshifts[i].percent) {
			cshift_changed = true;
			vs->prev_cshifts[i].percent = vs->cshifts[i].percent;
		}
		for (j = 0; j < 3; j++) {
			if (vs->cshifts[i].destcolor[j] != vs->prev_cshifts[i].destcolor[j])
			{
				cshift_changed = true;
				vs->prev_cshifts[i].destcolor[j] = vs->cshifts[i].destcolor[j];
			}
		}
	}

	// drop the damage value
	V_DropCShift (&vs->cshifts[CSHIFT_DAMAGE], vs->time, 150);
	// drop the bonus value
	V_DropCShift (&vs->cshifts[CSHIFT_BONUS], vs->time, 100);

	if (!cshift_changed)
		return;

	V_CalcBlend (vs);
}

/* VIEW RENDERING */

static void
CalcGunAngle (viewstate_t *vs)
{
	vec4f_t     rotation = Transform_GetWorldRotation (vs->camera_transform);
	//FIXME make child of camera
	transform_t wep_form = Entity_Transform (vs->weapon_entity);
	Transform_SetWorldRotation (wep_form, rotation);
}

static void
V_BoundOffsets (viewstate_t *vs)
{
	vec4f_t     offset = Transform_GetWorldPosition (vs->camera_transform);
	offset -= vs->player_origin;

	// absolutely bound refresh reletive to entity clipping hull
	// so the view can never be inside a solid wall

	offset[0] = bound (-14, offset[0], 14);
	offset[1] = bound (-14, offset[1], 14);
	offset[2] = bound (-22, offset[2], 30);
	Transform_SetWorldPosition (vs->camera_transform,
								vs->player_origin + offset);
}

static vec4f_t
idle_quat (vec4f_t axis, float cycle, float level, double time)
{
	vec4f_t     identity = { 0, 0, 0, 1 };
	if (!level || !cycle) {
		return identity;
	}
	float       scale = sin (time * cycle);
	float       ang = scale * level * v_idlescale;
	float       c = cos (ang * M_PI / 360);
	float       s = sin (ang * M_PI / 360);
	return axis * s + identity * c;
}

/*
	V_AddIdle

	Idle swaying
*/
static void
V_AddIdle (viewstate_t *vs)
{
	vec4f_t     roll = idle_quat ((vec4f_t) { 1, 0, 0, 0},
								  v_iroll_cycle, v_iroll_level, vs->time);
	vec4f_t     pitch = idle_quat ((vec4f_t) { 0, 1, 0, 0},
								   v_ipitch_cycle, v_ipitch_level, vs->time);
	vec4f_t     yaw = idle_quat ((vec4f_t) { 0, 0, 1, 0},
								 v_iyaw_cycle, v_iyaw_level, vs->time);
	vec4f_t     rot = normalf (qmulf (yaw, qmulf (pitch, roll)));

	// rotate the view
	vec4f_t     rotation = Transform_GetWorldRotation (vs->camera_transform);
	Transform_SetWorldRotation (vs->camera_transform, qmulf (rot, rotation));

	// counter-rotate the weapon
	transform_t wep_form = Entity_Transform (vs->weapon_entity);
	rot = qmulf (qconjf (rot), Transform_GetWorldRotation (wep_form));
	Transform_SetWorldRotation (wep_form, rot);
}

/*
	V_CalcViewRoll

	Roll is induced by movement and damage
*/
static void
V_CalcViewRoll (viewstate_t *vs)
{
	vec_t      *angles = vs->player_angles;
	vec4f_t     velocity = vs->velocity;
	vec3_t      ang = { };

	ang[ROLL] = V_CalcRoll (angles, velocity);

	if (v_dmg_time > 0) {
		ang[ROLL] += v_dmg_time / v_kicktime * v_dmg_roll;
		ang[PITCH] += v_dmg_time / v_kicktime * v_dmg_pitch;
		v_dmg_time -= vs->frametime;
	}

	if (vs->flags & VF_DEAD) {		// VF_GIB will also set VF_DEAD
		ang[ROLL] = 80;	// dead view angle
	}

	vec4f_t     rot;
	AngleQuat (ang, (vec_t*)&rot);//FIXME
	vec4f_t     rotation = Transform_GetWorldRotation (vs->camera_transform);
	Transform_SetWorldRotation (vs->camera_transform, qmulf (rotation, rot));
}

static void
V_CalcIntermissionRefdef (viewstate_t *vs)
{
	// vs->player_entity is the player model (visible when out of body)
	entity_t    ent = vs->player_entity;
	entity_t    view;
	float       old;
	transform_t transform = Entity_Transform (ent);
    vec4f_t     origin = Transform_GetWorldPosition (transform);
    vec4f_t     rotation = Transform_GetWorldRotation (transform);

	// view is the weapon model (visible only from inside body)
	view = vs->weapon_entity;

	Transform_SetWorldPosition (vs->camera_transform, origin);
	Transform_SetWorldRotation (vs->camera_transform, rotation);
	renderer_t *renderer = Ent_GetComponent (view.id, scene_renderer, view.reg);
	renderer->model = NULL;

	// always idle in intermission
	old = v_idlescale;
	v_idlescale = 1;
	V_AddIdle (vs);
	v_idlescale = old;
}

static void
V_CalcRefdef (viewstate_t *vs)
{
	// view is the weapon model (visible only from inside body)
	entity_t    view = vs->weapon_entity;
	float       bob;
	static float oldz = 0;
	vec4f_t     forward = {}, right = {}, up = {};
	vec4f_t     origin = vs->player_origin;
	vec_t      *viewangles = vs->player_angles;

	renderer_t *renderer = Ent_GetComponent (view.id, scene_renderer, view.reg);
	animation_t *animation = Ent_GetComponent (view.id, scene_animation, view.reg);

	V_DriftPitch (vs);

	bob = V_CalcBob (vs);

	// refresh position
	if (vs->flags & VF_GIB) {
		origin[2] += 8;					// gib view height
	} else if (vs->flags & VF_DEAD) {
		origin[2] += -16;				// corpse view height
	} else {
		origin[2] += vs->height + bob;
	}

	// never let it sit exactly on a node line, because a water plane can
	// disappear when viewed with the eye exactly on it.
	// server protocol specifies to only 1/8 pixel, so add 1/16 in each axis
	origin += (vec4f_t) { 1.0/16, 1.0/16, 1.0/16, 0};

	vec4f_t     rotation;
	AngleQuat (vs->player_angles, (vec_t*)&rotation);//FIXME
	Transform_SetWorldRotation (vs->camera_transform, rotation);
	V_CalcViewRoll (vs);
	V_AddIdle (vs);

	// offsets
	//FIXME semi-duplicates AngleQuat (also, vec3_t vs vec4f_t)
	AngleVectors (viewangles, (vec_t*)&forward, (vec_t*)&right, (vec_t*)&up);//FIXME

	// don't allow cheats in multiplayer
	// FIXME check for dead
	if (vs->voffs_enabled) {
		origin += scr_ofsx * forward + scr_ofsy * right + scr_ofsz * up;
	}

	V_BoundOffsets (vs);

	// set up gun position
	vec4f_t     gun_origin = vs->player_origin;
	CalcGunAngle (vs);

	gun_origin += (vec4f_t) { 0, 0, vs->height, 0 };
	gun_origin += forward * bob * 0.4f + (vec4f_t) { 0, 0, bob, 0 };

	// fudge position around to keep amount of weapon visible
	// roughly equal with different FOV
	if (hud_sbar == 0 && *r_data->scr_viewsize >= 100) {
		;
	} else if (*r_data->scr_viewsize == 110) {
		gun_origin += (vec4f_t) { 0, 0, 1, 0};
	} else if (*r_data->scr_viewsize == 100) {
		gun_origin += (vec4f_t) { 0, 0, 2, 0};
	} else if (*r_data->scr_viewsize == 90) {
		gun_origin += (vec4f_t) { 0, 0, 1, 0};
	} else if (*r_data->scr_viewsize == 80) {
		gun_origin += (vec4f_t) { 0, 0, 0.5, 0};
	}

	model_t    *model = vs->weapon_model;
	if (vs->flags & (VF_GIB | VF_DEAD)) {
		model = NULL;
	}
	if (renderer->model != model) {
		animation->pose2 = -1;
	}
	renderer->model = model;
	animation->frame = vs->weaponframe;
	renderer->skin = 0;

	// set up the refresh position
	Transform_SetWorldRotation (vs->camera_transform,
								qmulf (vs->punchangle, rotation));

	// smooth out stair step ups
	if ((vs->onground != -1) && (gun_origin[2] - oldz > 0)) {
		float       steptime;

		steptime = vs->frametime;

		oldz += steptime * 80;
		if (oldz > gun_origin[2])
			oldz = gun_origin[2];
		if (gun_origin[2] - oldz > 12)
			oldz = gun_origin[2] - 12;
		origin[2] += oldz - gun_origin[2];
		gun_origin[2] += oldz - gun_origin[2];
	} else {
		oldz = gun_origin[2];
	}
	Transform_SetWorldPosition (vs->camera_transform, origin);
	{
		// FIXME sort out the alias model specific negation
		vec3_t ang = {-viewangles[0], viewangles[1], viewangles[2]};
		CL_TransformEntity (view, 1, ang, gun_origin);
	}
}

static void
DropPunchAngle (viewstate_t *vs)
{
	vec4f_t     punch = vs->punchangle;
	float       ps = magnitude3f (punch)[0];
	if (ps < 1e-3) {
		// < 0.2 degree rotation, not worth worrying about
		//ensure the quaternion is normalized
		vs->punchangle = (vec4f_t) { 0, 0, 0, 1 };
		return;
	}
	float       pc = punch[3];
	float       ds = 0.0871557427 * vs->frametime;
	float       dc = sqrt (1 - ds * ds);
	float       s = ps * dc - pc * ds;
	float       c = pc * dc + ps * ds;
	if (s <= 0 || c >= 1) {
		vs->punchangle = (vec4f_t) { 0, 0, 0, 1 };
	} else {
		punch *= s / ps;
		punch[3] = c;
	}
}

/*
	V_RenderView

	The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
	the entity origin, so any view position inside that will be valid
*/
void
V_RenderView (viewstate_t *vs)
{
	qfZoneNamed (zone, true);
	if (!vs->active) {
		if (Transform_Valid (vs->camera_transform)) {
			vec4f_t     base = { 0, 0, 0, 1 };
			Transform_SetWorldPosition (vs->camera_transform, base);
			Transform_SetWorldRotation (vs->camera_transform, base);
		}
		return;
	}

	if (vs->decay_punchangle) {
		DropPunchAngle (vs);
	}
	if (vs->intermission) {				// intermission / finale rendering
		V_CalcIntermissionRefdef (vs);
	} else {
		if (vs->chase && chase_active) {
			Chase_Update (vs->chasestate);
		} else {
			V_CalcRefdef (vs);
		}
	}
}

void
V_NewScene (viewstate_t *viewstate, scene_t *scene)
{
	viewstate->camera_transform = Transform_New (cl_world.scene->reg,
												 nulltransform);
}

void
V_Init (viewstate_t *viewstate)
{
	Cmd_AddDataCommand ("bf", V_BonusFlash_f, viewstate,
						"Background flash, used when you pick up an item");
	Cmd_AddDataCommand ("centerview", V_StartPitchDrift_f, viewstate,
					"Centers the player's "
					"view ahead after +lookup or +lookdown\n"
					"Will not work while mlook is active or freelook is 1.");
	Cmd_AddCommand ("v_cshift", V_cshift_f, "This adjusts all of the colors "
					"currently being displayed.\n"
					"Used when you are underwater, hit, have the Ring of "
					"Shadows, or Quad Damage. (v_cshift r g b intensity)");
}

void
V_Init_Cvars (void)
{
	Cvar_Register (&v_centermove_cvar, 0, 0);
	Cvar_Register (&v_centerspeed_cvar, 0, 0);
	Cvar_Register (&v_iyaw_cycle_cvar, 0, 0);
	Cvar_Register (&v_iroll_cycle_cvar, 0, 0);
	Cvar_Register (&v_ipitch_cycle_cvar, 0, 0);
	Cvar_Register (&v_iyaw_level_cvar, 0, 0);
	Cvar_Register (&v_iroll_level_cvar, 0, 0);
	Cvar_Register (&v_ipitch_level_cvar, 0, 0);
	Cvar_Register (&v_idlescale_cvar, 0, 0);

	Cvar_Register (&scr_ofsx_cvar, 0, 0);
	Cvar_Register (&scr_ofsy_cvar, 0, 0);
	Cvar_Register (&scr_ofsz_cvar, 0, 0);
	Cvar_Register (&cl_rollspeed_cvar, 0, 0);
	Cvar_Register (&cl_rollangle_cvar, 0, 0);
	Cvar_Register (&cl_bob_cvar, 0, 0);
	Cvar_Register (&cl_bobcycle_cvar, 0, 0);
	Cvar_Register (&cl_bobup_cvar, 0, 0);
	Cvar_Register (&v_kicktime_cvar, 0, 0);
	Cvar_Register (&v_kickroll_cvar, 0, 0);
	Cvar_Register (&v_kickpitch_cvar, 0, 0);
	Cvar_Register (&cl_cshift_bonus_cvar, 0, 0);
	Cvar_Register (&cl_cshift_contents_cvar, 0, 0);
	Cvar_Register (&cl_cshift_damage_cvar, 0, 0);
	Cvar_Register (&cl_cshift_powerup_cvar, 0, 0);
}
