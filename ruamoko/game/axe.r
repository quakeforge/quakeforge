#include "axe.h"
#include "gameent.h"
#include "message.h"
#include "sound.h"
#include "tempent.h"
#include "trace.h"
#include "vector.h"
#include "world.h"

@implementation Axe

-init
{
	[super init];
	damage = (deathmatch > 3) ? 75.0 : 20.0;
	return self;
}

-setOwner:(Entity)o
{
	owner = o;
	return self;
}

-fire
{
	local entity s = [owner ent];
	local vector org, source;

	makevectors (s.v_angle);
	source = s.origin + '0 0 16';
	traceline (source, source + v_forward * 64, NO, s);
	if (trace_fraction == 1.0)
		return self;

	org = trace_endpos - v_forward * 4;

	if ([trace_ent.@this takeDamage:self inflictor:s attacker:s :damage])
		SpawnBlood (org, 20);
	else {
		sound (s, CHAN_WEAPON, "player/axhit2.wav", 1, ATTN_NORM);

		WriteByte (MSG_MULTICAST, SVC_TEMPENTITY);
		WriteByte (MSG_MULTICAST, TE_GUNSHOT);
		WriteByte (MSG_MULTICAST, 3);
		WriteCoord (MSG_MULTICAST, org_x);
		WriteCoord (MSG_MULTICAST, org_y);
		WriteCoord (MSG_MULTICAST, org_z);
		multicast (org, MULTICAST_PVS);
	}
	return self;
}

@end
