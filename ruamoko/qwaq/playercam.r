#include "playercam.h"

motor_t camera_lookat (point_t eye, point_t target, point_t up);

@implementation PlayerCam
-init
{
	if (!(self = [super init])) {
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

+playercam
{
	return [[[self alloc] init] autorelease];
}

-think:(float)frametime
{
	auto M = camera_lookat (nest, focus, up);
	auto delta = ~state.M * M;
	auto log_delta = log (delta);
	state.M *= exp (log_delta * 0.1);
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

-(state_t)state
{
	return state;
}
@end
