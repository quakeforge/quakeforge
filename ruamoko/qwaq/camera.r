#include <draw.h>
#include <math.h>

#include "camera.h"
#include "gizmo.h"
#include "qwaq-ed.h"

@implementation Camera

-initInScene:(scene_t)scene
{
	if (!(self = [super init])) {
		return nil;
	}

	self.scene = scene;
	ent = Scene_CreateEntity (scene);
	xform = Entity_GetTransform (ent);
	return self;
}

+(Camera *) inScene:(scene_t)scene
{
	return [[[Camera alloc] initInScene:scene] autorelease];
}

-(void)dealloc
{
	[self error:"dealloc"];
	Scene_DestroyEntity (ent);
	[super dealloc];
}

-(entity_t) entity
{
	return ent;
}

-setTransformFromMotor:(motor_t)M
{
	set_transform (M, xform);
	return self;
}

-draw
{
	auto mat = Transform_GetWorldMatrix (xform);
	vec4 x = mat[0];
	vec4 y = mat[1];
	vec4 z = mat[2];
	vec4 p = mat[3];
	Gizmo_AddCapsule (p, p + x, 0.025, vec4(1, 0, 0, 0.2));
	Gizmo_AddCapsule (p, p + y, 0.025, vec4(0, 1, 0, 0.2));
	Gizmo_AddCapsule (p, p + z, 0.025, vec4(0, 0, 1, 0.2));
	return self;
}

-drawExcept:(Camera *) skip
{
	if (skip != self) {
		return [self draw];
	}
	return self;
}

@end

void
camera_first_person (state_t *state)
{
	vector dpos = {};
	dpos.x -= IN_UpdateAxis (cam_move_forward);
	dpos.y -= IN_UpdateAxis (cam_move_side);
	dpos.z -= IN_UpdateAxis (cam_move_up);

	vector drot = {};
	drot.x -= IN_UpdateAxis (cam_move_roll);
	drot.y -= IN_UpdateAxis (cam_move_pitch);
	drot.z -= IN_UpdateAxis (cam_move_yaw);

	dpos *= 0.01;
	drot *= ((float)M_PI / 360);
	state.B = {
		.bvect = (PGA.bvect) drot,
		.bvecp = (PGA.bvecp) dpos,
	};
	//use a constant dt instead of actual frame time because the input
	//values accumulate over time and thus already include frame time.
	float h = 0.02f;
	auto ds = dState (*state);
	state.M += h * ds.M;
	state.B += h * ds.B;
	state.M = normalize (state.M);
}

void
camera_mouse_first_person (state_t *state)
{
	vector dpos = {};
	vector drot = {};
	drot.y -= IN_UpdateAxis (mouse_y);
	drot.z -= IN_UpdateAxis (mouse_x);

	dpos *= 0.1;
	drot *= ((float)M_PI / 36);
	state.B = {
		.bvect = (PGA.bvect) drot,
		.bvecp = (PGA.bvecp) dpos,
	};
	//use a constant dt instead of actual frame time because the input
	//values accumulate over time and thus already include frame time.
	float h = 0.02f;
	auto ds = dState (*state);
	state.M += h * ds.M;
	state.B += h * ds.B;
	state.M = normalize (state.M);
}

static PGA.vec
trackball_vector (vec2 xy)
{
	// xy is already in -1..1 order of magnitude range (ie, might be a bit
	// over due to screen aspect)
	// This is very similar to blender's trackball calculation (based on it,
	// really)
	float r = 1;
	float t = r * r / 2;
	float d = xy • xy;
	vec3 vec = vec3 (xy, 0);
	if (d < t) {
		// less than 45 degrees around the sphere from the viewer facing
		// pole, so map the mouse point to the sphere
		vec.z = sqrt (r * r - d);
	} else {
		// beyond 45 degrees around the sphere from the veiwer facing
		// pole, so the slope is rapidly approaching infinity or the mouse
		// point may miss the sphere entirely, so instead map the mouse
		// point to the hyperbolic cone pointed towards the viewer. The
		// cone and sphere are tangential at the 45 degree latitude
		vec.z = t / sqrt (d);
	}
	auto v = vec4(vec.zxy, 0);
	auto p = (PGA.vec) v;
	return p;
}

@overload float
min (float x, float y)
{
	return (x) < (y) ? (x) : (y);
}

@overload float
max (float x, float y)
{
	return (x) > (y) ? (x) : (y);
}

static float trackball_sensitivity = 10.0f;
#define sphere_scale 1.0f
void
camera_mouse_trackball (state_t *state)
{
	vec2 delta = {
		IN_UpdateAxis (mouse_x),
		IN_UpdateAxis (mouse_y),
	};
	int         width = Draw_Width ();
	int         height = Draw_Height ();

	float       size = min (width, height) / 2;
	vec2        center = vec2 (width, height) / 2;
	vec2        m_start = mouse_start - center;
	vec2        m_end = m_start + delta;
	auto        start = trackball_vector (m_start / (size * sphere_scale));
	auto        end = trackball_vector (m_end / (size * sphere_scale));
	bivector_t  drot = (start ∧ end) * 0.5f;
	@algebra(PGA) {
		auto p = e123 + 5 * e032;
		state.B = (drot • p) * p;
	}
	auto ds = dState (*state);
	state.M += ds.M;
	state.B += ds.B;
	state.M = normalize (state.M);
}
