#include "camera.h"
#include "gizmo.h"

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

@end
