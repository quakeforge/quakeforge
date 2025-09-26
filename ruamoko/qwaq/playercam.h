#ifndef __playercam_h
#define __playercam_h

#include <Object.h>

#include "camera.h"
#include "pga3d.h"

@interface PlayerCam : Camera
{
	point_t focus;
	point_t nest;
	point_t up;
	state_t state;
}
+(PlayerCam *) inScene:(scene_t)scene;
-think:(float)frametime;
-setFocus:(point_t)focus;
-setNest:(point_t)nest;
-(point_t)getNest;
-(state_t)state;
@end

#endif//__playercam_h
