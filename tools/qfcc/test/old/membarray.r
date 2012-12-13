#include "Entity.h"

@class Waypoint;
@class Bot;

struct bot_data_t = {
	string name;
	float pants, shirt;
};
typedef struct bot_data_t bot_data_t;

@interface Target: Entity
{
@public
	Waypoint current_way;
	Target _last;
}
@end

@interface Waypoint: Target
{
@public
	Waypoint [4] links;
	Waypoint []list;
	unsigned count;
	unsigned b_sound;
}
-(void)fix;
-(void)clearRouteForBot:(Bot)bot;
@end

@interface Bot: Target
{
@public
	integer b_clientflag;
}
@end

@implementation Target
@end

@implementation Waypoint
-(void)fix
{
	//local integer i = 2, tmp;
	unsigned u;
	//tmp = (integer)links[i];
	for (u = 0; u < count; u++)
		[list[u] fix];
}

-(void)clearRouteForBot:(Bot)bot
{
	local integer flag;
	flag = bot.b_clientflag;
	b_sound &= ~flag;
}
@end
