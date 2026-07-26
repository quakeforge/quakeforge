#ifndef __camera_h
#define __camera_h

#include <Object.h>

#include "pga3d.h"

typedef @handle(long) scene_h scene_t;
typedef @handle(long) entity_h entity_t;
typedef @handle(long) transform_h transform_t;

@interface Camera : Object
{
	scene_t     scene;
	entity_t    ent;
	transform_t xform;
	bool        hud_enabled;
	state_t     state;
}
+(Camera *) inScene:(scene_t)scene;
-(entity_t) entity;
-setState:(state_t)state;
-(state_t)state;
-setTransformFromMotor:(motor_t)M;
-draw;
-drawExcept:(Camera *) skip;
-drawHUD;
-enableHUD:(bool)enable;
@end

typedef struct camspeed_s camspeed_t;

void camera_first_person (state_t *camera_state, camspeed_t speed);
void camera_mouse_trackball (state_t *camera_state, vec2 mouse_start);
void camera_mouse_first_person (state_t *camera_state);

#endif//__camera_h
