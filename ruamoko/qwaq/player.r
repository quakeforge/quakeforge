#include <math.h>
#include "player.h"

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

-initInScene:(scene_t) scene
{
	if (!(self = [super init])) {
		return nil;
	}
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

	onground = true;
	velocity = '0 0 0';

	IMP imp = [self methodForSelector: @selector (jump:)];
	IN_ButtonAddListener (move_jump, imp, self);

	return self;
}

-(void)dealloc
{
	IMP imp = [self methodForSelector: @selector (jump)];
	IN_ButtonRemoveListener (move_jump, imp, self);
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

-think:(float)frametime
{
	vector dpos = {};
	dpos.x -= IN_UpdateAxis (move_forward);
	dpos.y -= IN_UpdateAxis (move_side);
	dpos *= 2;

	vector pos = Transform_GetLocalPosition (xform).xyz;
	onground = pos.z <= 0 && velocity.z <= 0;
	vector a = '0 0 0';
	if (onground) {
		pos.z = 0;
		velocity.z = 0;
		velocity.x = dpos.x;
		velocity.y = dpos.y;
	} else {
		a = '0 0 -1' * 9.81f;
	}
	velocity += a * frametime;
	pos += (velocity - 0.5 * a * frametime) * frametime;
	Transform_SetLocalPosition (xform, vec4 (pos, 1));

	if (dpos • dpos) {
		vector fwd = Transform_Forward (xform).xyz;
		quaternion drot = fromtorot (fwd, dpos);
		quaternion rot = Transform_GetLocalRotation (xform);
		Transform_SetLocalRotation (xform, drot * rot);
	}
	return self;
}

-(point_t)pos
{
	return (point_t)Transform_GetWorldPosition (xform);
}
@end
