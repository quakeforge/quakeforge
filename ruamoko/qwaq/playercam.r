#include <math.h>
#include "playercam.h"

motor_t camera_lookat (point_t eye, point_t target, point_t up);

@implementation PlayerCam

-initInScene:(scene_t)scene
{
	if (!(self = [super initInScene:scene])) {
		return nil;
	}

	@algebra (PGA) {
		focus = e032;	// infinity in the +X direction
		up = e021;		// infinity in the +Z direction
		nest = e123;	// at the origin
	};
	auto M = camera_lookat (nest, focus, up);
	state = { .M = M };
	return self;
}

+(PlayerCam *) inScene:(scene_t)scene
{
	return [[[PlayerCam alloc] initInScene:scene] autorelease];
}

+playercam
{
	return [[[self alloc] init] autorelease];
}

-think:(float)frametime
{
	auto M = camera_lookat (nest, focus, up);
	auto delta = ~state.M * M;
	if (delta.scalar < 0) {
		delta = -delta;
	}
	auto log_delta = log (delta);
	state.M *= exp (log_delta * 0.1);

	[self setTransformFromMotor: state.M];
	return self;
}

-setFocus:(point_t)focus
{
	self.focus = focus;
	return self;
}

-setNest:(point_t)nest
{
	self.nest = nest;
	return self;
}

-(point_t)getNest
{
	return nest;
}

-(state_t)state
{
	return state;
}
@end
