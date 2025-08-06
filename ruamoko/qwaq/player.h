#ifndef __player_h
#define __player_h

#include <Object.h>
#include <scene.h>

#include "pga3d.h"

#include <input.h>
extern in_axis_t *move_forward;
extern in_axis_t *move_side;
extern in_axis_t *move_up;
extern in_axis_t *move_pitch;
extern in_axis_t *move_yaw;
extern in_axis_t *move_roll;
extern in_button_t *move_jump;
extern in_button_t *target_lock;

void add_target (entity_t tgt);

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

-setCamera:(PlayerCam *)camera;
-think:(float)frametime;
-(point_t)pos;
@end

#endif//__player_h
