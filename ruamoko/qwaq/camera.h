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
}
+(Camera *) inScene:(scene_t)scene;
-(entity_t) entity;
-setTransformFromMotor:(motor_t)M;
-draw;
@end

#endif//__camera_h
