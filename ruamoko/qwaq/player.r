#include <math.h>
#include "player.h"
#include "playercam.h"

#define MAX_TARGETS 16
int num_targets;
entity_t targets[MAX_TARGETS];

void add_target (entity_t tgt)
{
	if (num_targets < MAX_TARGETS) {
		targets[num_targets++] = tgt;
	}
}

@implementation Player
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

	ent = Scene_CreateEntity (scene);
	auto body = Scene_CreateEntity (scene);
	auto mask = Scene_CreateEntity (scene);
	Entity_SetModel (body, Model_Load ("progs/Capsule.mdl"));
	Entity_SetModel (mask, Model_Load ("progs/Cube.mdl"));

	xform = Entity_GetTransform (ent);
	auto body_xform = Entity_GetTransform (body);
	auto mask_xform = Entity_GetTransform (mask);
	Transform_SetParent (body_xform, xform);
	Transform_SetParent (mask_xform, body_xform);
	Transform_SetLocalPosition(xform, {-5, 0, 0, 1});
	Transform_SetLocalPosition(body_xform, {0, 0, 1, 1});
	Transform_SetLocalPosition(mask_xform, {0.5, 0, 0.5, 1});
	Transform_SetLocalScale(mask_xform, {0.25, 0.5, 0.125, 1});

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
	Scene_DestroyEntity (ent);
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
	vector dpos = {};
	dpos.x -= IN_UpdateAxis (move_forward);
	dpos.y -= IN_UpdateAxis (move_side);
	dpos *= 2;

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
	auto p = vec4 (pos, 1);
	Transform_SetLocalPosition (self.xform, p);

	if (dp • dp) {
		vector fwd = Transform_Forward (self.xform).xyz;
		quaternion drot = fromtorot (fwd, vector (dp, 0));
		quaternion rot = Transform_GetLocalRotation (self.xform);
		Transform_SetLocalRotation (self.xform, drot * rot);
	}
	return (point_t)p;
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

-think:(float)frametime
{
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
@end
