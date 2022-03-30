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
#include "QF/scene/transform.h"
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

cvar_t     *scr_ofsx;
cvar_t     *scr_ofsy;
cvar_t     *scr_ofsz;

cvar_t     *cl_rollspeed;
cvar_t     *cl_rollangle;

cvar_t     *cl_bob;
cvar_t     *cl_bobcycle;
cvar_t     *cl_bobup;

cvar_t     *v_centermove;
cvar_t     *v_centerspeed;

cvar_t     *v_kicktime;
cvar_t     *v_kickroll;
cvar_t     *v_kickpitch;

cvar_t     *cl_cshift_bonus;
cvar_t     *cl_cshift_contents;
cvar_t     *cl_cshift_damage;
cvar_t     *cl_cshift_powerup;

cvar_t     *v_iyaw_cycle;
cvar_t     *v_iroll_cycle;
cvar_t     *v_ipitch_cycle;
cvar_t     *v_iyaw_level;
cvar_t     *v_iroll_level;
cvar_t     *v_ipitch_level;

cvar_t     *v_idlescale;

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

	value = cl_rollangle->value;

	if (side < cl_rollspeed->value)
		side = side * value / cl_rollspeed->value;
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
	cycle = bobtime - (int) (bobtime / cl_bobcycle->value) *
		cl_bobcycle->value;
	cycle /= cl_bobcycle->value;
	if (cycle < cl_bobup->value)
		cycle = cycle / cl_bobup->value;
	else
		cycle = 1 + (cycle - cl_bobup->value) / (1.0 - cl_bobup->value);

	// bob is proportional to velocity in the xy plane
	// (don't count Z, or jumping messes it up)
	velocity[2] = 0;
	bob = sqrt (dotf (velocity, velocity)[0]) * cl_bob->value;
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
		vs->pitchvel = v_centerspeed->value;
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
		if (fabs (forwardmove) < cl_forwardspeed->value)
			vs->driftmove = 0;
		else
			vs->driftmove += vs->frametime;

		if (vs->driftmove > v_centermove->value) {
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
	vs->pitchvel += vs->frametime * v_centerspeed->value;

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

	if (cl_cshift_damage->int_val
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
	v_dmg_roll = count * side * v_kickroll->value;

	side = DotProduct (from, forward);
	v_dmg_pitch = count * side * v_kickpitch->value;

	v_dmg_time = v_kicktime->value;
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
	if (!cl_cshift_bonus->int_val
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
	if (!cl_cshift_contents->int_val
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
	int         i, j;

	if (cl_cshift_powerup->int_val
		|| (vs->force_cshifts & INFO_CSHIFT_POWERUP))
		V_CalcPowerupCshift (vs);

	qboolean    cshift_changed = false;

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
	Transform_SetWorldRotation (vs->weapon_entity->transform, rotation);
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
idle_quat (vec4f_t axis, cvar_t *cycle, cvar_t *level, double time)
{
	vec4f_t     identity = { 0, 0, 0, 1 };
	if (!level || !cycle) {
		return identity;
	}
	float       scale = sin (time * cycle->value);
	float       ang = scale * level->value * v_idlescale->value;
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
	rot = qmulf (qconjf (rot),
				 Transform_GetWorldRotation (vs->weapon_entity->transform));
	Transform_SetWorldRotation (vs->weapon_entity->transform, rot);
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
		ang[ROLL] += v_dmg_time / v_kicktime->value * v_dmg_roll;
		ang[PITCH] += v_dmg_time / v_kicktime->value * v_dmg_pitch;
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
	entity_t   *ent = vs->player_entity;
	entity_t   *view;
	float       old;
    vec4f_t     origin = Transform_GetWorldPosition (ent->transform);
    vec4f_t     rotation = Transform_GetWorldRotation (ent->transform);

	// view is the weapon model (visible only from inside body)
	view = vs->weapon_entity;

	Transform_SetWorldPosition (vs->camera_transform, origin);
	Transform_SetWorldRotation (vs->camera_transform, rotation);
	view->renderer.model = NULL;

	// always idle in intermission
	old = v_idlescale->value;
	Cvar_SetValue (v_idlescale, 1);
	V_AddIdle (vs);
	Cvar_SetValue (v_idlescale, old);
}

static void
V_CalcRefdef (viewstate_t *vs)
{
	// view is the weapon model (visible only from inside body)
	entity_t   *view = vs->weapon_entity;
	float       bob;
	static float oldz = 0;
	vec4f_t     forward = {}, right = {}, up = {};
	vec4f_t     origin = vs->player_origin;
	vec_t      *viewangles = vs->player_angles;

	V_DriftPitch (vs);

	bob = V_CalcBob (vs);

	// refresh position
	origin[2] += vs->height + bob;

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
		origin += scr_ofsx->value * forward
										+ scr_ofsy->value * right
										+ scr_ofsz->value * up;
	}

	V_BoundOffsets (vs);

	// set up gun position
	vec4f_t     gun_origin = vs->player_origin;
	CalcGunAngle (vs);

	gun_origin += (vec4f_t) { 0, 0, vs->height, 0 };
	gun_origin += forward * bob * 0.4f + (vec4f_t) { 0, 0, bob, 0 };

	// fudge position around to keep amount of weapon visible
	// roughly equal with different FOV
	if (hud_sbar->int_val == 0 && r_data->scr_viewsize->int_val >= 100) {
		;
	} else if (r_data->scr_viewsize->int_val == 110) {
		gun_origin += (vec4f_t) { 0, 0, 1, 0};
	} else if (r_data->scr_viewsize->int_val == 100) {
		gun_origin += (vec4f_t) { 0, 0, 2, 0};
	} else if (r_data->scr_viewsize->int_val == 90) {
		gun_origin += (vec4f_t) { 0, 0, 1, 0};
	} else if (r_data->scr_viewsize->int_val == 80) {
		gun_origin += (vec4f_t) { 0, 0, 0.5, 0};
	}

	model_t    *model = vs->weapon_model;
	if (vs->flags & (VF_GIB | VF_DEAD)) {
		model = NULL;
	}
	if (view->renderer.model != model) {
		view->animation.pose2 = -1;
	}
	view->renderer.model = model;
	view->animation.frame = vs->weaponframe;
	view->renderer.skin = 0;

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
	if (!vs->active) {
		vec4f_t     base = { 0, 0, 0, 1 };
		Transform_SetWorldPosition (vs->camera_transform, base);
		Transform_SetWorldRotation (vs->camera_transform, base);
		return;
	}

	if (vs->decay_punchangle) {
		DropPunchAngle (vs);
	}
	if (vs->intermission) {				// intermission / finale rendering
		V_CalcIntermissionRefdef (vs);
	} else {
		if (vs->chase && chase_active->int_val) {
			Chase_Update (vs->chasestate);
		} else {
			V_CalcRefdef (vs);
		}
	}
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

	viewstate->camera_transform = Transform_New (cl_world.scene, 0);
}

void
V_Init_Cvars (void)
{
	v_centermove = Cvar_Get ("v_centermove", "0.15", CVAR_NONE, NULL,
							 "How far the player must move forward before the "
							 "view re-centers");
	v_centerspeed = Cvar_Get ("v_centerspeed", "500", CVAR_NONE, NULL,
							  "How quickly you return to a center view after "
							  "a lookup or lookdown");
	v_iyaw_cycle = Cvar_Get ("v_iyaw_cycle", "2", CVAR_NONE, NULL,
							 "How far you tilt right and left when "
							 "v_idlescale is enabled");
	v_iroll_cycle = Cvar_Get ("v_iroll_cycle", "0.5", CVAR_NONE, NULL,
							  "How quickly you tilt right and left when "
							  "v_idlescale is enabled");
	v_ipitch_cycle = Cvar_Get ("v_ipitch_cycle", "1", CVAR_NONE, NULL,
							   "How quickly you lean forwards and backwards "
							   "when v_idlescale is enabled");
	v_iyaw_level = Cvar_Get ("v_iyaw_level", "0.3", CVAR_NONE, NULL,
							 "How far you tilt right and left when "
							 "v_idlescale is enabled");
	v_iroll_level = Cvar_Get ("v_iroll_level", "0.1", CVAR_NONE, NULL,
							  "How far you tilt right and left when "
							  "v_idlescale is enabled");
	v_ipitch_level = Cvar_Get ("v_ipitch_level", "0.3", CVAR_NONE, NULL,
							   "How far you lean forwards and backwards when "
							   "v_idlescale is enabled");
	v_idlescale = Cvar_Get ("v_idlescale", "0", CVAR_NONE, NULL,
							"Toggles whether the view remains idle");

	scr_ofsx = Cvar_Get ("scr_ofsx", "0", CVAR_NONE, NULL, "None");
	scr_ofsy = Cvar_Get ("scr_ofsy", "0", CVAR_NONE, NULL, "None");
	scr_ofsz = Cvar_Get ("scr_ofsz", "0", CVAR_NONE, NULL, "None");
	cl_rollspeed = Cvar_Get ("cl_rollspeed", "200", CVAR_NONE, NULL,
							 "How quickly you straighten out after strafing");
	cl_rollangle = Cvar_Get ("cl_rollangle", "2.0", CVAR_NONE, NULL,
							 "How much your screen tilts when strafing");
	cl_bob = Cvar_Get ("cl_bob", "0.02", CVAR_NONE, NULL,
					   "How much your weapon moves up and down when walking");
	cl_bobcycle = Cvar_Get ("cl_bobcycle", "0.6", CVAR_NONE, NULL,
							"How quickly your weapon moves up and down when "
							"walking");
	cl_bobup = Cvar_Get ("cl_bobup", "0.5", CVAR_NONE, NULL,
						 "How long your weapon stays up before cycling when "
						 "walking");
	v_kicktime = Cvar_Get ("v_kicktime", "0.5", CVAR_NONE, NULL,
						   "How long the kick from an attack lasts");
	v_kickroll = Cvar_Get ("v_kickroll", "0.6", CVAR_NONE, NULL,
						   "How much you lean when hit");
	v_kickpitch = Cvar_Get ("v_kickpitch", "0.6", CVAR_NONE, NULL,
							"How much you look up when hit");
	cl_cshift_bonus = Cvar_Get ("cl_cshift_bonus", "1", CVAR_ARCHIVE, NULL,
								"Show bonus flash on item pickup");
	cl_cshift_contents = Cvar_Get ("cl_cshift_content", "1", CVAR_ARCHIVE,
								   NULL, "Shift view colors for contents "
								   "(water, slime, etc)");
	cl_cshift_damage = Cvar_Get ("cl_cshift_damage", "1", CVAR_ARCHIVE, NULL,
								 "Shift view colors on damage");
	cl_cshift_powerup = Cvar_Get ("cl_cshift_powerup", "1", CVAR_ARCHIVE, NULL,
								  "Shift view colors for powerups");
}
