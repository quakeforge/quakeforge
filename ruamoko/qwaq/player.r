#include <math.h>
#include <input.h>
#include <PropertyList.h>
#include "animfsm.h"
#include "physics.h"
#include "player.h"
#include "playercam.h"
#include "qwaq-ed.h"

void printf(string fmt, ...);

static string player_plist =
#embed "config/player.plist"
;

in_axis_t *move_forward;
in_axis_t *move_side;
in_axis_t *move_up;
in_axis_t *move_pitch;
in_axis_t *move_yaw;
in_axis_t *move_roll;
in_axis_t *look_forward;
in_axis_t *look_right;
in_axis_t *look_up;
in_button_t *shift;
in_button_t *move_jump;
in_button_t *target_lock;

#define MAX_TARGETS 16
int num_targets;
entity_t targets[MAX_TARGETS];

void add_target (entity_t tgt)
{
	if (num_targets < MAX_TARGETS) {
		targets[num_targets++] = tgt;
	}
}

@overload vector log (quaternion q)
{
	float a = (q * ~q).w;
	if (!a) {
		return '0 0 0';
	}
	float s = sqrt (a);
	float b = atan2 (s, q.s) / s;
	return q.v * b;
}

static @overload quaternion exp (vector v)
{
	float l = v • v;
	if (l == 0) {
		return '0 0 0 1';
	}
	float a = sqrt (l);
	auto sc = sincos (a);
	sc[0] /= a;
	return [v * sc[0], sc[1]];
}

@implementation Player
+(void)create_bindings
{
	move_forward = IN_CreateAxis ("move.forward", "Player Move Fore/Aft");
	move_side = IN_CreateAxis ("move.side", "Player Move Left/Right");
	move_up = IN_CreateAxis ("move.up", "Player Move Up/Down");
	move_pitch = IN_CreateAxis ("move.pitch", "Player Pitch");
	move_yaw = IN_CreateAxis ("move.yaw", "Player Yaw");
	move_roll = IN_CreateAxis ("move.roll", "Player Roll");
	shift = IN_CreateButton ("shift", "Player shift");
	move_jump = IN_CreateButton ("move.jump", "Player Jump");
	look_forward = IN_CreateAxis ("look.forward", "Player Look Forward");
	look_right = IN_CreateAxis ("look.right", "Player Look Right");
	look_up = IN_CreateAxis ("look.up", "Player Look Up");
	target_lock = IN_CreateButton ("target.lock", "Player Target Lock");
}

-(void) jump: (in_button_t *) button
{
	if (button.state & inb_edge_down) {
		if (onground) {
			velocity.z += sqrt(10.0f);
			onground = false;
		}
		button.state &= inb_down;
	}
}

-(void) target_lock: (in_button_t *) button
{
	if (button.state & inb_edge_down) {
		button.state &= inb_down;
		if (!marker) {
			auto fwd = view;
			auto pos = [self pos];
			auto fwd_dir = pos ∨ fwd;
			fwd_dir /= sqrt (fwd_dir • ~fwd_dir);
			float best_dot = 0;
			entity_t best_tgt = nil;
			for (int i = 0; i < num_targets; i++) {
				auto xf = Entity_GetTransform (targets[i]);
				auto tgt = (point_t) Transform_GetWorldPosition (xf);
				auto dir = pos ∨ tgt;
				dir /= dir • ~dir;
				float dot = dir • fwd_dir;
				if (dot < best_dot) {
					best_dot = dot;
					best_tgt = targets[i];
				}
			}
			if (best_tgt) {
				marker = Scene_CreateEntity (scene);
				auto tx = Entity_GetTransform (best_tgt);
				auto mx = Entity_GetTransform (marker);
				Transform_SetParent (mx, tx);
				Transform_SetLocalPosition (mx, {0, 0, 0.5, 1});
				Entity_SetModel (marker, hexhair);
			}
		} else {
			Scene_DestroyEntity (marker);
			marker = nil;
			view_dist = cam_dist;
		}
	}
}

-initInScene:(scene_t) scene
{
	if (!(self = [super init])) {
		return nil;
	}
	self.scene = scene;

	auto ctx = [PLArchive archive];
	[ctx add:self withId:"player"];
	auto player_item = [PLItem fromString:player_plist];
	q_ent = load_entity (player_item, scene, ctx);
	[ctx finalize];

	get_component (q_ent, qent_fsm, &fsm);
	get_component (q_ent, qent_entity, &s_ent);
	get_component (q_ent, qent_transform, &xform);
	//FIXME rotation
	Transform_SetLocalRotation(xform, { 0, 0, 0.707106769, 0.707106769});
	Transform_SetLocalPosition(xform, {-5, 0, 0, 1});
	[[fsm setEntity:s_ent] start];

	hexhair = Model_Load ("progs/hexhair.mdl");

	onground = true;
	velocity = '0 0 0';

	IMP imp = [self methodForSelector: @selector (jump:)];
	IN_ButtonAddListener (move_jump, imp, self);
	imp = [self methodForSelector: @selector (target_lock:)];
	IN_ButtonAddListener (target_lock, imp, self);

	pitch = '1 0';
	yaw   = '1 0';
	cam_dist = 3;
	view_dist = cam_dist;

	@algebra (PGA) {
		chest = 1.5 * e021;
	}

	return self;
}

-(void)dealloc
{
	IMP imp = [self methodForSelector: @selector (jump)];
	IN_ButtonRemoveListener (move_jump, imp, self);
	imp = [self methodForSelector: @selector (target_lock:)];
	IN_ButtonRemoveListener (target_lock, imp, self);
	Scene_DestroyEntity (s_ent);
	[super dealloc];
}

+player:(scene_t) scene
{
	return [[[self alloc] initInScene:scene] autorelease];
}

quaternion
fromtorot(vector a, vector b)
{
	float ma = sqrt (a • a);
	float mb = sqrt (b • b);
	vector mb_a = mb * a;
	vector ma_b = ma * b;
	float den = 2 * ma * mb;
	float mba_mab = sqrt ((mb_a + ma_b) • (mb_a + ma_b));
	float c = mba_mab / den;
	vector v = (a × b) / mba_mab;
	if (mba_mab < 0.001 && a • b < 0) {
		v = '0 0 1';
		c = 0;
	}
	return quaternion(v.x, v.y, v.z, c);
}

point_t
Player_move (Player *self, float frametime)
{
	vector dpos = self.wish_vel;

	float r = self.view_dist * self.pitch.x;
	float v = fabs (dpos.y) * frametime;
	if (v > r) { v = r; }
	float s = v / (2 * r);
	float c = sqrt (1 - s * s);

	if (self.marker) {
		dpos.x += fabs(dpos.y) * s;
	} else {
		dpos.x -= fabs(dpos.y) * s;
	}
	dpos.y *= c;

	vector pos = Transform_GetLocalPosition (self.xform).xyz;
	self.onground = pos.z <= 0 && self.velocity.z <= 0;
	vector a = '0 0 0';
	vec2 dp = {
		.x = dpos.x * self.yaw.x - dpos.y * self.yaw.y,
		.y = dpos.x * self.yaw.y + dpos.y * self.yaw.x,
	};
	if (self.onground) {
		pos.z = 0;
		self.velocity.z = 0;
		self.velocity.x = dp.x;
		self.velocity.y = dp.y;

		auto y = self.yaw;
		if (dpos.y < 0) {
			s = -s;
		}
		self.yaw = { y.x * c - y.y * s, y.x * s + y.y * c };
	} else {
		a = '0 0 -1' * 9.81f;
	}
	self.velocity += a * frametime;
	pos += (self.velocity - 0.5 * a * frametime) * frametime;
	auto p = point_t (pos, 1);
	[self.fsm enableRootMotion:self.onground];
	if (!self.onground) {
		Transform_SetLocalPosition (self.xform, (vec4)p);
	}

	if (dp • dp) {
		//FIXME rotation
		vector fwd = Transform_Right (self.xform).xyz;
		//vector fwd = Transform_Forward (xform).xyz;
		quaternion drot = fromtorot (fwd, vector (dp, 0));
		drot = exp (self.turn_frac * log (drot));
		quaternion rot = Transform_GetLocalRotation (self.xform);
		Transform_SetLocalRotation (self.xform, drot * rot);

		auto r = ~(rotor_t) drot;
		auto m = (r • p) * ~p;
		[self.fsm pivotRoot:m];
	}
	return p;
}

void
Player_freecam (Player *self, float frametime)
{
	vector drot = {};
	//drot.x -= IN_UpdateAxis (move_roll);
	drot.y -= IN_UpdateAxis (move_pitch);
	drot.z -= IN_UpdateAxis (move_yaw);
	drot *= 5 * frametime;

	vec2 dp = {-self.pitch.y, self.pitch.x };
	self.pitch += dp * drot.y;
	if (self.pitch.y >  0.996194698) { self.pitch.y =  0.996194698; }
	if (self.pitch.y < -0.996194698) { self.pitch.y = -0.996194698; }
	if (self.pitch.x < 0.0871557427) { self.pitch.x = 0.0871557427; }

	vec2 dy = {-self.yaw.y, self.yaw.x };
	self.yaw += dy * drot.z;

	vec3 dir = {
		-IN_UpdateAxis (look_forward),
		-IN_UpdateAxis (look_right),
		-IN_UpdateAxis (look_up),
	};
	if (dir.xy) {
		self.yaw = dir.xy;
		self.pitch = {1, 0};
	}
	if (dir.z) {
		self.pitch = {1, dir.z};
	}

	self.yaw /= sqrt (self.yaw • self.yaw);
	self.pitch /= sqrt (self.pitch • self.pitch);

	// +pitch causes the view to tilt *down* which is correct for +Y left in
	// a right-handed system)
	self.view = (point_t) vec4(self.yaw * self.pitch.x, -self.pitch.y, 0);
}

-collect_input
{
	vector dpos = {};
	dpos.x -= IN_UpdateAxis (move_forward);
	dpos.y -= IN_UpdateAxis (move_side);
	dpos.z -= IN_UpdateAxis (move_up);
	float mag = sqrt(dpos • dpos);
	if (mag < 1) {
		mag = 1;
	}
	wish_vel = dpos / mag;
	return self;
}

//FIXME [exact_types] attribute (functions and methods) so floats can be
//passed through ...
//FIXME instant events at the beginning of an animation get lost
//for now, doubles as left-contact
-(void)clip_end:(id)sender id:(uint)id start:(double)start end:(double)end
{
	//printf ("%@ %@ %d %g %g\n", self, sender, id, start, end);
}

-(void)contact:(id)sender id:(uint)id start:(double)start end:(double)end
{
	//printf ("contact %s\n", id < 6 ? "left" : "right");
}

-(void)heel:(id)sender id:(uint)id start:(double)start end:(double)end
{
	//printf ("heel %s\n", id < 6 ? "left" : "right");
	if (id < 6) {
		left_contact = true;
	} else {
		right_contact = true;
	}
	turn_frac = float (end - start) * ((left_contact && right_contact) ? 1 : 0.2f);
}

-(void)flat:(id)sender id:(uint)id start:(double)start end:(double)end
{
	//printf ("flat %s\n", id < 6 ? "left" : "right");
	if (id < 6) {
		left_contact = true;
	} else {
		right_contact = true;
	}
	turn_frac = float(end - start) * ((left_contact && right_contact) ? 1 : 0.5f);
}

-(void)toes:(id)sender id:(uint)id start:(double)start end:(double)end
{
	//printf ("toes %s\n", id < 6 ? "left" : "right");
	if (id < 6) {
		left_contact = true;
	} else {
		right_contact = true;
	}
	turn_frac = float (end - start) * ((left_contact && right_contact) ? 1 : 0.2f);
}

-(void)flight:(id)sender id:(uint)id start:(double)start end:(double)end
{
	//printf ("flight %s\n", !(id & 1) ? "left" : "right");
	turn_frac = 0;
	left_contact = false;
	right_contact = false;
}

-think:(float)frametime
{
	[self collect_input];

	if (marker) {
		auto mx = Entity_GetTransform (marker);
		auto mp = (point_t) Transform_GetWorldPosition (mx);
		auto dir = [self pos] ∨ mp;
		view_dist = sqrt (dir • ~dir);
		dir /= view_dist;
		yaw = ((vec3)dir.bvect).xy;
		pitch.x = sqrt (yaw • yaw);
		pitch.y = -((vec3)dir.bvect).z;
		yaw /= pitch.x;
		self.view = (point_t) vec4(yaw * pitch.x, -pitch.y, 0);
	} else {
		Player_freecam (self, frametime);
	}
	[fsm update:frametime];
	auto pos = Player_move (self, frametime);

	[camera setFocus:view];
	point_t nest = pos + chest - cam_dist * view;
	if (marker) {
		@algebra (PGA) {
			nest += yaw.y * e032 - yaw.x * e013;
		}
	}
	[camera setNest:nest];

	return self;
}

-(point_t)pos
{
	return (point_t)Transform_GetWorldPosition (xform) + chest;
}

-setCamera:(PlayerCam *)camera
{
	self.camera = camera;
	return self;
}

-(bool)checkIdle
{
	float wv2 = wish_vel • wish_vel;
	return wv2 < 0.015625;
}

-(bool)checkWalk
{
	float wv2 = wish_vel • wish_vel;
	return wv2 >= 0.015625 && wv2 < 0.25;
}

-(bool)checkRun
{
	float wv2 = wish_vel • wish_vel;
	return wv2 >= 0.25;
}
@end
