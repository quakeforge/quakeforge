#ifndef __camerasystem_h
#define __camerasystem_h

#include <Object.h>

@class Array;
@class Camera;

typedef struct in_axis_s in_axis_t;
typedef struct in_button_s in_button_t;

extern in_axis_t *cam_move_forward;
extern in_axis_t *cam_move_side;
extern in_axis_t *cam_move_up;
extern in_axis_t *cam_move_pitch;
extern in_axis_t *cam_move_yaw;
extern in_axis_t *cam_move_roll;
extern in_button_t *cam_next;
extern in_button_t *cam_prev;

extern in_axis_t *mouse_x;
extern in_axis_t *mouse_y;

typedef struct mousestate_s {
	vec2        drag_start;
	bool        dragging_mmb;
	bool        dragging_rmb;
} mousestate_t;

typedef struct camspeed_s {
	float       speed;
	float       sensitivity;
} camspeed_t;

@interface CameraSystem : Object
{
	Camera     *active_camera;
	uint        active_camera_index;
	Array      *cameras;
	scene_t     scene;

	camspeed_t  speed;
}
+(CameraSystem *)cameraSystem:(scene_t)scene;
+(void)create_bindings;
-addCamera:(Camera *)camera;
-(Camera *)camera;
-updateCamera:(mousestate_t)mouse;
@end

#include "gui/window.h"

@class Slider;

@interface CamWindow : Window
{
	CameraSystem *system;
	float camera_speed_exp;
	float sensitivity;

	Slider *exp_slider;
	Slider *sens_slider;
}
+(CamWindow *) camWindow:(CameraSystem*)sys ctx:(imui_ctx_t)ctx;
-draw;
@end

#endif//__camerasystem_h
