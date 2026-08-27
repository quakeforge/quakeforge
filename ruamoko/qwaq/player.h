#ifndef __player_h
#define __player_h

#include <Object.h>
#include <scene.h>

#include "pga3d.h"

#include <input.h>

void add_target (entity_t tgt);
quaternion fromtorot(vector a, vector b);

@class PlayerCam;

@interface Player : Object
{
	scene_t scene;
	entity_t ent;
	transform_t xform;

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
}
+player:(scene_t) scene;
+(void)create_bindings;

-setCamera:(PlayerCam *)camera;
-think:(float)frametime;
-(point_t)pos;
@end

#endif//__player_h
