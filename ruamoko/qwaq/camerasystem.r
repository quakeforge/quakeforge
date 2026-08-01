#include <Array.h>
#include <input.h>
#include <imui.h>
#include <math.h>
#include <string.h>

#include "camera.h"
#include "camerasystem.h"

#include "gui/slider.h"

in_axis_t *cam_move_forward;
in_axis_t *cam_move_side;
in_axis_t *cam_move_up;
in_axis_t *cam_move_pitch;
in_axis_t *cam_move_yaw;
in_axis_t *cam_move_roll;
in_button_t *cam_next;
in_button_t *cam_prev;
in_button_t *cam_hud;

in_axis_t *mouse_x;
in_axis_t *mouse_y;

//FIXME having PGA.group_mask(0xc) here and then providing a defintion causes
//a segfault in qfcc
@generic (genObj = [PGA.group_mask(0xe)]) {

genObj
sqrt (genObj x)
{
	auto a = x + 1;
	return a / sqrt (a • ~a);
}

};

@overload
PGA.group_mask(0xc)
sqrt (PGA.group_mask(0xc) x)
{
	return (x + x.scalar) / 2;
}

//void
motor_t
camera_lookat (point_t eye, point_t target, point_t up)
{
//sqrt( b / a ) = +- b * normalize( b + a )
	@algebra (PGA) {
		point_t eye_0 = e123;
		point_t eye_fwd = e032;
		point_t eye_up = e021;
		auto l0 = (eye_0 ∨ eye_fwd);
		auto p0 = (eye_0 ∨ eye_fwd ∨ eye_up);

		auto l = -(eye ∨ target);
		auto p = (eye ∨ target ∨ up);
		float f = (p • p) / (l•~l);
		if (f < 0.005) {
			// looking (nearly) parallel (or anti-parallel) to the up vector,
			// so fall back (smoothly) to the reference plane
			f = f / 0.005;
			p = f * p + (1 - f) * p0;
		}
		p /= sqrt (p • p);
		l /= sqrt (l • ~l);

		auto T = sqrt(-eye * eye_0);
		auto Tm = l * T * l0 * ~T;
		Tm = normalize (Tm);
		motor_t R;
		if (Tm.scalar < -0.5) {
			// looking backwards along the reference forward direction
			// Rotate 180 around an axis in the reference plane that's
			// perpendicular to the reference forward direction, calculate
			// the rotation to get to that, then undo the 180 degree rotation
			auto A = ((⋆(p0 * e0123) ∧ ⋆(l0 * e0)) • eye) * eye;
			if ((A • Tm).scalar < 0) {
				A = ~A;
			}
			Tm = A * l * ~A * T * l0 * ~T;
			Tm = normalize (Tm);
			R = ~A * sqrt(Tm) * T;
		} else {
			R = sqrt(Tm) * T;
		}
		//FIXME scalar+bvect isn't accepted by full motors for normalize
		motor_t pp = p * R * p0 * ~R;
		auto Rm = normalize (pp);
		motor_t L;
		if (Rm.scalar < -0.5) {
			// The target plane is "almost" anti-parallel to the reference
			// plane, so rotate it 180 around the target line, calculate the
			// needed rotation, then undo the 180 degree rotation
			p = l * p * ~l;
			pp = p * R * p0 * ~R;
			Rm = normalize (pp);
			L = ~l * sqrt(Rm) * R;
		} else {
			L = sqrt(Rm) * R;
		}
		return normalize (L);
	}
}

@implementation CameraSystem

+(void)create_bindings
{
	cam_move_forward = IN_CreateAxis ("cam.move.forward", "Camera Fore/Aft");
	cam_move_side = IN_CreateAxis ("cam.move.side", "Camera Left/Right");
	cam_move_up = IN_CreateAxis ("cam.move.up", "Camera Up/Down");
	cam_move_pitch = IN_CreateAxis ("cam.move.pitch", "Camera Pitch");
	cam_move_yaw = IN_CreateAxis ("cam.move.yaw", "Camera Yaw");
	cam_move_roll = IN_CreateAxis ("cam.move.roll", "Camera Roll");
	cam_next = IN_CreateButton ("cam.next", "Camera Next");
	cam_prev = IN_CreateButton ("cam.prev", "Camera Prev");
	cam_hud = IN_CreateButton ("cam.hud", "Camera Hud");

	mouse_x = IN_CreateAxis ("mouse.x", "Mouse X");
	mouse_y = IN_CreateAxis ("mouse.y", "Mouse Y");
}

-init:(scene_t)scene
{
	if (!(self = [super init])) {
		return nil;
	}

	self.scene = scene;
	cameras = [[Array array] retain];

	// create a default camera
	active_camera_index = [cameras count];
	active_camera = [[Camera inScene:scene] retain];
	[cameras addObject:active_camera];
	Scene_SetCamera (scene, [active_camera entity]);

	IMP imp;
	imp = [self methodForSelector: @selector (nextCamera:)];
	IN_ButtonAddListener (cam_next, imp, self);
	imp = [self methodForSelector: @selector (prevCamera:)];
	IN_ButtonAddListener (cam_prev, imp, self);

	speed = { 1, 1 };

	//FIXME
	[active_camera setState:{
		.M = make_motor ({ -4, 0, 3, 0, }, { 0, 0.316227766, 0, 0.948683298 }),
		//.M = make_motor ({ -16, 12, 10, 0, },
		//		{ -0.223606797, 0.223606797, 0.670820393, 0.670820393 }),
	}];
	return self;
}

+(CameraSystem *) cameraSystem:(scene_t)scene
{
	return [[[CameraSystem alloc] init:scene] autorelease];
}

-(void)dealloc
{
	[self error:"dealloc"];
	[super dealloc];
}

-addCamera:(Camera *) camera
{
	[cameras addObject:camera];
	return self;
}

-(void) nextCamera:(in_button_t *)button
{
	if (button.state & inb_edge_down) {
		button.state &= inb_down;

		uint old_cam = active_camera_index;
		if (++active_camera_index >= [cameras count]) {
			active_camera_index = 0;
		}
		if (active_camera_index != old_cam) {
			active_camera = [cameras objectAtIndex:active_camera_index];
			Scene_SetCamera (scene, [active_camera entity]);
		}
	}
}

-(void) prevCamera:(in_button_t *)button
{
	if (button.state & inb_edge_down) {
		button.state &= inb_down;

		uint old_cam = active_camera_index;
		if ((int) --active_camera_index < 0) {
			active_camera_index = [cameras count] - 1;
		}
		if (active_camera_index < 0) {
			[self error:"no cameras!"];
		}
		if (active_camera_index != old_cam) {
			active_camera = [cameras objectAtIndex:active_camera_index];
			Scene_SetCamera (scene, [active_camera entity]);
		}
	}
}

-updateCamera:(mousestate_t)mouse
{
	Camera *camera = [cameras objectAtIndex:0];
	auto state = [camera state];

	camera_first_person (&state, speed);
	if (mouse.dragging_mmb) {
		camera_mouse_trackball (&state, mouse.drag_start);
	}
	if (mouse.dragging_rmb) {
		camera_mouse_first_person (&state);
	}
	[camera setState:state];

	[camera enableHUD: cam_hud.state & inb_down];
	[camera drawHUD];

	[cameras makeObjectsPerformSelector: @selector(drawExcept:)
							 withObject: active_camera];
	return self;
}

-(Camera *) camera
{
	return active_camera;
}

-(void)setSpeed:(float)speed
{
	self.speed.speed = speed;
}

-(void)setSensitivity:(float)sensitivity
{
	self.speed.sensitivity = sensitivity;
}

-(vec3)forward
{
	Camera *camera = [cameras objectAtIndex:0];
	auto xform = [camera xform];
	return Transform_Forward (xform).xyz;
}
@end

@implementation CamWindow
-initWithContext:(CameraSystem*)sys ctx:(imui_ctx_t)ctx
{
	if (!(self = [super initWithContext:ctx name:"CamWindow"])) {
		return nil;
	}
	system = [sys retain];
	IMUI_Window_SetSize (window, {300, 150});

	exp_slider = [[Slider slider:{0, 6} step:0.05 ctx:ctx] retain];
	sens_slider = [[Slider slider:{-3, 0} step:0.05 ctx:ctx] retain];
	camera_speed_exp = 0;
	return self;
}

+(CamWindow *) camWindow:(CameraSystem*)sys ctx:(imui_ctx_t)ctx
{
	return [[[CamWindow alloc] initWithContext:sys ctx:ctx] autorelease];
}

-draw
{
	if (![super draw]) {
		return nil;
	}
	UI_Window (window) {
		if (IMUI_Window_IsCollapsed (window)) {
			continue;
		}
		UI_Vertical {
			if ([exp_slider draw:camera_speed_exp]) {
				[system setSpeed:pow (10, camera_speed_exp)];
			}
			IMUI_Labelf (IMUI_context, "%4.1f##camWindow_sp", camera_speed_exp);
		}
		//FIXME sort out why Slider needs free positioning
		UI_Vertical {
			if ([sens_slider draw:sensitivity]) {
				[system setSensitivity:pow (10, sensitivity)];
			}
			IMUI_Labelf (IMUI_context, "%4.1f##camWindow_sn", sensitivity);
			UI_Horizontal {
				vec3 forward = [system forward];
				vec2 xy = forward.xy;
				float ra = atan2 (forward.y, forward.x) * 12 / M_PI;
				float decl = atan2 (forward.z, sqrt (xy • xy)) * 180 / M_PI;
				if (ra < 0) {
					ra += 24;
				}
				int ra_h = int (ra);
				int ra_m = int ((ra - ra_h) * 60);
				float ra_s = ((ra - ra_h) * 60 - ra_m) * 60;
				float sgn = decl < 0 ? -1 : 1;
				decl = fabs (decl);
				int decl_d = int (decl);
				int decl_m = int (fabs ((decl - decl_d) * 60));
				float decl_s = ((decl - decl_d) * 60 - decl_m) * 60;
				string ra_str = sprintf ("%2dh%02dm%04.1fs", ra_h, ra_m, ra_s);
				string decl_str = sprintf ("%3.0f⁰%02d'%04.1f\"",
										   sgn * float(decl_d), decl_m, decl_s);
				IMUI_Labelf (IMUI_context, "%s##camWindow_aim_ra", ra_str);
				UI_FlexibleSpace ();
				IMUI_Labelf (IMUI_context, "%s##camWindow_aim_de", decl_str);
			}
		}
	}
	return self;
}
@end
