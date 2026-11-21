#ifndef __player_h
#define __player_h

#include <Object.h>
#include <scene.h>

#include "pga3d.h"

void add_target (entity_t tgt);
quaternion fromtorot(vector a, vector b);

@class PlayerCam;
@class AnimFSM;;

@interface Player : Object
{
	scene_t scene;
	uint q_ent;
	entity_t s_ent;
	transform_t xform;

	AnimFSM *fsm;

	model_t hexhair;
	entity_t marker;

	point_t chest;
	point_t view;

	bool onground;
	vector velocity;

	vec2 pitch;
	vec2 yaw;
	float cam_dist;
	float view_dist;

	PlayerCam *camera;

	vector wish_vel;
}
+player:(scene_t) scene;
+(void)create_bindings;

-setCamera:(PlayerCam *)camera;
-think:(float)frametime;
-(point_t)pos;
@end

#endif//__player_h
