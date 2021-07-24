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
#include "QF/entity.h"
#include "QF/msg.h"
#include "QF/screen.h"

#include "QF/simd/vec4f.h"

#include "QF/plugin/vid_render.h"

#include "compat.h"

#include "nq/include/chase.h"
#include "nq/include/client.h"
#include "nq/include/host.h"

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

cshift_t    cshift_empty = { {130, 80, 50}, 0};
cshift_t    cshift_water = { {130, 80, 50}, 128};
cshift_t    cshift_slime = { {0, 25, 5}, 150};
cshift_t    cshift_lava = { {255, 80, 0}, 150};
cshift_t    cshift_bonus = { {215, 186, 60}, 50};

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
V_CalcBob (void)
{
	vec4f_t     velocity = cl.viewstate.velocity;
	float       cycle;
	static double bobtime;
	static float bob;

	if (cl.spectator)
		return 0;

	if (cl.viewstate.onground == -1)
		return bob;						// just use old value

	bobtime += cl.viewstate.frametime;
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
V_StartPitchDrift (void)
{
	if (cl.laststop == cl.time) {
		return;					// something else is keeping it from drifting
	}

	if (cl.nodrift || !cl.pitchvel) {
		cl.pitchvel = v_centerspeed->value;
		cl.nodrift = false;
		cl.driftmove = 0;
	}
}

void
V_StopPitchDrift (void)
{
	cl.laststop = cl.time;
	cl.nodrift = true;
	cl.pitchvel = 0;
}

/*
	V_DriftPitch

	Moves the client pitch angle towards cl.idealpitch sent by the server.

	If the user is adjusting pitch manually, either with lookup/lookdown,
	mlook and mouse, or klook and keyboard, pitch drifting is constantly
	stopped.

	Drifting is enabled when the center view key is hit, mlook is released
	and lookspring is non 0, or when
*/
static void
V_DriftPitch (void)
{
	float       delta, move;
	float       forwardmove = cl.viewstate.movecmd[0];

	if (noclip_anglehack || cl.viewstate.onground == -1 || cls.demoplayback) {
		cl.driftmove = 0;
		cl.pitchvel = 0;
		return;
	}

	// don't count small mouse motion
	if (cl.nodrift) {
		if (fabs (forwardmove) < cl_forwardspeed->value)
			cl.driftmove = 0;
		else
			cl.driftmove += cl.viewstate.frametime;

		if (cl.driftmove > v_centermove->value) {
			V_StartPitchDrift ();
		}
		return;
	}

	delta = cl.idealpitch - cl.viewstate.angles[PITCH];

	if (!delta) {
		cl.pitchvel = 0;
		return;
	}

	move = cl.viewstate.frametime * cl.pitchvel;
	cl.pitchvel += cl.viewstate.frametime * v_centerspeed->value;

	if (delta > 0) {
		if (move > delta) {
			cl.pitchvel = 0;
			move = delta;
		}
		cl.viewstate.angles[PITCH] += move;
	} else if (delta < 0) {
		if (move > -delta) {
			cl.pitchvel = 0;
			move = -delta;
		}
		cl.viewstate.angles[PITCH] -= move;
	}
}

/* PALETTE FLASHES */

void
V_ParseDamage (void)
{
	float       count, side;
	int         armor, blood;
	vec4f_t     origin = cl.viewstate.origin;
	vec_t      *angles = cl.viewstate.angles;
	vec3_t      from, forward, right, up;

	armor = MSG_ReadByte (net_message);
	blood = MSG_ReadByte (net_message);
	MSG_ReadCoordV (net_message, from);

	count = blood * 0.5 + armor * 0.5;
	if (count < 10)
		count = 10;

	cl.faceanimtime = cl.time + 0.2;	// but sbar face into pain frame

	if (cl_cshift_damage->int_val
		|| (cl.sv_cshifts & INFO_CSHIFT_DAMAGE)) {
		cshift_t   *cshift = &cl.cshifts[CSHIFT_DAMAGE];

		cshift->percent += 3 * count;
		cshift->percent =
			bound (0, cshift->percent, 150);

		if (armor > blood) {
			cshift->destcolor[0] = 200;
			cshift->destcolor[1] = 100;
			cshift->destcolor[2] = 100;
		} else if (armor) {
			cshift->destcolor[0] = 220;
			cshift->destcolor[1] = 50;
			cshift->destcolor[2] = 50;
		} else {
			cshift->destcolor[0] = 255;
			cshift->destcolor[1] = 0;
			cshift->destcolor[2] = 0;
		}
		cshift->initialpct = cshift->percent;
		cshift->time = cl.time;
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
V_BonusFlash_f (void)
{
	if (!cl_cshift_bonus->int_val
		&& !(cl.sv_cshifts & INFO_CSHIFT_BONUS))
		return;

	cl.cshifts[CSHIFT_BONUS] = cshift_bonus;
	cl.cshifts[CSHIFT_BONUS].initialpct = cl.cshifts[CSHIFT_BONUS].percent;
	cl.cshifts[CSHIFT_BONUS].time = cl.time;
}

/*
	V_SetContentsColor

	Underwater, lava, etc each has a color shift
*/
void
V_SetContentsColor (int contents)
{
	if (!cl_cshift_contents->int_val
		&& !(cl.sv_cshifts & INFO_CSHIFT_CONTENTS)) {
		cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
		return;
	}

	switch (contents) {
		case CONTENTS_EMPTY:
			cl.cshifts[CSHIFT_CONTENTS] = cshift_empty;
			break;
		case CONTENTS_LAVA:
			cl.cshifts[CSHIFT_CONTENTS] = cshift_lava;
			break;
		case CONTENTS_SOLID:
		case CONTENTS_SLIME:
			cl.cshifts[CSHIFT_CONTENTS] = cshift_slime;
			break;
		default:
			cl.cshifts[CSHIFT_CONTENTS] = cshift_water;
	}
}

static void
V_CalcPowerupCshift (void)
{
	if (!cl.stats[STAT_ITEMS] & (IT_SUIT || IT_INVISIBILITY || IT_QUAD
								 || IT_INVULNERABILITY))
	{
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
		return;
	}

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY &&
		cl.stats[STAT_ITEMS] & IT_QUAD) {
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	} else if (cl.stats[STAT_ITEMS] & IT_QUAD) {
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 255;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	} else if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 30;
	} else if (cl.stats[STAT_ITEMS] & IT_SUIT) {
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 0;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 255;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 0;
		cl.cshifts[CSHIFT_POWERUP].percent = 20;
	} else if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		cl.cshifts[CSHIFT_POWERUP].destcolor[0] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[1] = 100;
		cl.cshifts[CSHIFT_POWERUP].destcolor[2] = 100;
		cl.cshifts[CSHIFT_POWERUP].percent = 100;
	} else {
		cl.cshifts[CSHIFT_POWERUP].percent = 0;
	}
}

/*
  V_CalcBlend

  LordHavoc made this a real, true alpha blend.  Cleaned it up
  a bit, but otherwise this is his code.  --KB
*/
static void
V_CalcBlend (void)
{
	float       a2, a3;
	float       r = 0, g = 0, b = 0, a = 0;
	int         i;

	for (i = 0; i < NUM_CSHIFTS; i++) {
		a2 = cl.cshifts[i].percent / 255.0;

		if (!a2)
			continue;

		a2 = min (a2, 1.0);
		r += (cl.cshifts[i].destcolor[0] - r) * a2;
		g += (cl.cshifts[i].destcolor[1] - g) * a2;
		b += (cl.cshifts[i].destcolor[2] - b) * a2;

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

	r_data->vid->cshift_color[0] = min (r, 255.0) / 255.0;
	r_data->vid->cshift_color[1] = min (g, 255.0) / 255.0;
	r_data->vid->cshift_color[2] = min (b, 255.0) / 255.0;
	r_data->vid->cshift_color[3] = bound (0.0, a, 1.0);
}

static void
V_DropCShift (cshift_t *cs, float droprate)
{
	if (cs->time < 0) {
		  cs->percent = 0;
	} else {
		cs->percent = cs->initialpct - (cl.time - cs->time) * droprate;
		if (cs->percent  <= 0) {
			cs->percent = 0;
			cs->time = -1;
		}
	}
}

void
V_PrepBlend (void)
{
	int         i, j;

	if (cl_cshift_powerup->int_val
		|| (cl.sv_cshifts & INFO_CSHIFT_POWERUP))
		V_CalcPowerupCshift ();

	r_data->vid->cshift_changed = false;

	for (i = 0; i < NUM_CSHIFTS; i++) {
		if (cl.cshifts[i].percent != cl.prev_cshifts[i].percent) {
			r_data->vid->cshift_changed = true;
			cl.prev_cshifts[i].percent = cl.cshifts[i].percent;
		}
		for (j = 0; j < 3; j++) {
			if (cl.cshifts[i].destcolor[j] != cl.prev_cshifts[i].destcolor[j])
			{
				r_data->vid->cshift_changed = true;
				cl.prev_cshifts[i].destcolor[j] = cl.cshifts[i].destcolor[j];
			}
		}
	}

	// drop the damage value
	V_DropCShift (&cl.cshifts[CSHIFT_DAMAGE], 150);
	// drop the bonus value
	V_DropCShift (&cl.cshifts[CSHIFT_BONUS], 100);

	if (!r_data->vid->cshift_changed && !r_data->vid->recalc_refdef)
		return;

	V_CalcBlend ();
}

/* VIEW RENDERING */

static void
CalcGunAngle (void)
{
	vec4f_t     rotation = r_data->refdef->viewrotation;
	//FIXME make child of camera
	Transform_SetWorldRotation (cl.viewent.transform, rotation);
}

static void
V_BoundOffsets (void)
{
	vec4f_t     offset = r_data->refdef->viewposition
						- cl.viewstate.origin;

	// absolutely bound refresh reletive to entity clipping hull
	// so the view can never be inside a solid wall

	offset[0] = bound (-14, offset[0], 14);
	offset[1] = bound (-14, offset[1], 14);
	offset[2] = bound (-22, offset[2], 30);
	r_data->refdef->viewposition = cl.viewstate.origin + offset;
}

static vec4f_t
idle_quat (vec4f_t axis, cvar_t *cycle, cvar_t *level)
{
	vec4f_t     identity = { 0, 0, 0, 1 };
	if (!level || !cycle) {
		return identity;
	}
	float       scale = sin (cl.time * cycle->value);
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
V_AddIdle (void)
{
	vec4f_t     roll = idle_quat ((vec4f_t) { 1, 0, 0, 0},
								  v_iroll_cycle, v_iroll_level);
	vec4f_t     pitch = idle_quat ((vec4f_t) { 0, 1, 0, 0},
								   v_ipitch_cycle, v_ipitch_level);
	vec4f_t     yaw = idle_quat ((vec4f_t) { 0, 0, 1, 0},
								 v_iyaw_cycle, v_iyaw_level);
	vec4f_t     rot = normalf (qmulf (yaw, qmulf (pitch, roll)));

	// rotate the view
	r_data->refdef->viewrotation = qmulf (rot, r_data->refdef->viewrotation);

	// counter-rotate the weapon
	rot = qmulf (qconjf (rot),
				 Transform_GetWorldRotation (cl.viewent.transform));
	Transform_SetWorldRotation (cl.viewent.transform, rot);
}

/*
	V_CalcViewRoll

	Roll is induced by movement and damage
*/
static void
V_CalcViewRoll (void)
{
	vec_t      *angles = cl.viewstate.angles;
	vec4f_t     velocity = cl.viewstate.velocity;
	vec3_t      ang = { };

	ang[ROLL] = V_CalcRoll (angles, velocity);

	if (v_dmg_time > 0) {
		ang[ROLL] += v_dmg_time / v_kicktime->value * v_dmg_roll;
		ang[PITCH] += v_dmg_time / v_kicktime->value * v_dmg_pitch;
		v_dmg_time -= cl.viewstate.frametime;
	}

	if (cl.viewstate.flags & VF_DEAD) {		// VF_GIB will also set VF_DEAD
		ang[ROLL] = 80;	// dead view angle
	}

	vec4f_t     rot;
	AngleQuat (ang, &rot[0]);//FIXME
	r_data->refdef->viewrotation = qmulf (r_data->refdef->viewrotation, rot);
}

static void
V_CalcIntermissionRefdef (void)
{
    // ent is the player model (visible when out of body)
    entity_t   *ent = &cl_entities[cl.viewentity];
	entity_t   *view;
	float       old;
    vec4f_t     origin = Transform_GetWorldPosition (ent->transform);
    vec4f_t     rotation = Transform_GetWorldRotation (ent->transform);

	// view is the weapon model (visible only from inside body)
	view = &cl.viewent;

	r_data->refdef->viewposition = origin;
	r_data->refdef->viewrotation = rotation;
	view->renderer.model = NULL;

	// always idle in intermission
	old = v_idlescale->value;
	Cvar_SetValue (v_idlescale, 1);
	V_AddIdle ();
	Cvar_SetValue (v_idlescale, old);
}

static void
V_CalcRefdef (void)
{
	// view is the weapon model (visible only from inside body)
	entity_t   *view = &cl.viewent;
	float       bob;
	static float oldz = 0;
	vec4f_t     forward = {}, right = {}, up = {};
	vec4f_t     origin = cl.viewstate.origin;
	vec_t      *viewangles = cl.viewstate.angles;

	V_DriftPitch ();

	bob = V_CalcBob ();

	// refresh position
	r_data->refdef->viewposition = origin;
	r_data->refdef->viewposition[2] += cl.viewheight + bob;

	// never let it sit exactly on a node line, because a water plane can
	// disappear when viewed with the eye exactly on it.
	// server protocol specifies to only 1/8 pixel, so add 1/16 in each axis
	r_data->refdef->viewposition += (vec4f_t) { 1.0/16, 1.0/16, 1.0/16, 0};

	AngleQuat (cl.viewstate.angles, &r_data->refdef->viewrotation[0]);//FIXME
	V_CalcViewRoll ();
	V_AddIdle ();

	// offsets
	//FIXME semi-duplicates AngleQuat (also, vec3_t vs vec4f_t)
	AngleVectors (viewangles, &forward[0], &right[0], &up[0]);

	// don't allow cheats in multiplayer
	// FIXME check for dead
	if (cl.maxclients == 1) {
		r_data->refdef->viewposition += scr_ofsx->value * forward
										+ scr_ofsy->value * right
										+ scr_ofsz->value * up;
	}

	V_BoundOffsets ();

	// set up gun position
	CalcGunAngle ();

	origin += (vec4f_t) { 0, 0, cl.viewheight, 0 };
	origin += forward * bob * 0.4f + (vec4f_t) { 0, 0, bob, 0 };

	// fudge position around to keep amount of weapon visible
	// roughly equal with different FOV
	if (hud_sbar->int_val == 0 && r_data->scr_viewsize->int_val >= 100) {
		;
	} else if (r_data->scr_viewsize->int_val == 110) {
		origin += (vec4f_t) { 0, 0, 1, 0};
	} else if (r_data->scr_viewsize->int_val == 100) {
		origin += (vec4f_t) { 0, 0, 2, 0};
	} else if (r_data->scr_viewsize->int_val == 90) {
		origin += (vec4f_t) { 0, 0, 1, 0};
	} else if (r_data->scr_viewsize->int_val == 80) {
		origin += (vec4f_t) { 0, 0, 0.5, 0};
	}

	view->renderer.model = cl.model_precache[cl.stats[STAT_WEAPON]];
	view->animation.frame = cl.stats[STAT_WEAPONFRAME];
	view->renderer.skin = 0;

	// set up the refresh position
	r_data->refdef->viewrotation = qmulf (cl.viewstate.punchangle,
										  r_data->refdef->viewrotation);

	// smooth out stair step ups
	if ((cl.viewstate.onground != -1) && (origin[2] - oldz > 0)) {
		float       steptime;

		steptime = cl.viewstate.frametime;

		oldz += steptime * 80;
		if (oldz > origin[2])
			oldz = origin[2];
		if (origin[2] - oldz > 12)
			oldz = origin[2] - 12;
		r_data->refdef->viewposition[2] += oldz - origin[2];
		origin[2] += oldz - origin[2];
	} else {
		oldz = origin[2];
	}
	{
		// FIXME sort out the alias model specific negation
		vec3_t ang = {-viewangles[0], viewangles[1], viewangles[2]};
		CL_TransformEntity (view, 1, ang, origin);
	}

	if (cl.chase && chase_active->int_val) {
		Chase_Update ();
	}
}

/*
	V_RenderView

	The player's clipping box goes from (-16 -16 -24) to (16 16 32) from
	the entity origin, so any view position inside that will be valid
*/
void
V_RenderView (void)
{
	if (cls.state != ca_active) {
		r_data->refdef->viewposition = (vec4f_t) { 0, 0, 0, 1 };
		r_data->refdef->viewrotation = (vec4f_t) { 0, 0, 0, 1 };
		return;
	}

	if (cl.intermission) {				// intermission / finale rendering
		V_CalcIntermissionRefdef ();
	} else {
		V_CalcRefdef ();
	}

	r_funcs->R_RenderView ();
}

void
V_Init (void)
{
	Cmd_AddCommand ("bf", V_BonusFlash_f, "Background flash, used when you "
					"pick up an item");
	Cmd_AddCommand ("centerview", V_StartPitchDrift, "Centers the player's "
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
}
