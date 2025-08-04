#include "playercam.h"

motor_t camera_lookat (point_t eye, point_t target, point_t up);

@implementation PlayerCam
-init
{
	if (!(self = [super init])) {
		return nil;
	}
	@algebra (PGA) {
		focus = e032;
		nest = e123;
	};
	state = { .M = { .scalar = 1 } };
	return self;
}

+playercam
{
	return [[[self alloc] init] autorelease];
}

-think
{
	auto M = camera_lookat (nest, focus, (point_t) '0 0 1 0');
	auto delta = M * ~state.M;
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
