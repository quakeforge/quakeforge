#ifndef __playercam_h
#define __playercam_h

#include <Object.h>

#include "pga3d.h"

@interface PlayerCam : Object
{
	point_t focus;
	point_t nest;
	point_t up;
	state_t state;
}
+playercam;
-think;
-setFocus:(point_t)focus;
-setNest:(point_t)nest;
-(state_t)state;
@end

#endif//__playercam_h
