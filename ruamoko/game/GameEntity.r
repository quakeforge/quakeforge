#include "GameEntity.h"

.vector angles;
.float modelindex;
.float movetype;
.string model;
.float frame;
.float colormap;
.vector mins;
.vector maxs;
.vector velocity;
.vector origin;
.float flags;
.vector v_angle;

@implementation GameEntity

- (BOOL) takeDamage: weapon : inflictor : attacker : damage
{
	return NO;
}
@end
