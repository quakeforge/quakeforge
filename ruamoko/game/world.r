#include "gameent.h"
#include "world.h"

id world;
integer deathmatch;

void(entity e, vector min, vector max) setsize = #0;
void(entity e, vector o) setorigin = #0;


#define MAX_BODIES 8

@interface BodyQue : Object
{
	entity [MAX_BODIES] bodies;
	integer head;
}
-init;
-add:(GameEntity)ent;
@end

@implementation BodyQue
-init
{
	local integer i;
	[super init];
	self.head = 0;
	for (i = 0; i < MAX_BODIES; i++)
		self.bodies[i] = [GameEntity new];
}

-add:(GameEntity)ent
{
	local entity be = bodies[head++];
	local entity e = [ent ent];
	be.angles = e.angles;
	be.model = e.model;
	be.modelindex = e.modelindex;
	be.frame = e.frame;
	be.colormap = e.colormap;
	be.movetype = e.movetype;
	be.velocity = e.velocity;
	be.flags = 0;
	setorigin (be, e.origin);
	setsize (be, e.mins, e.maxs);
	return self;
}
@end

@interface World : GameEntity
{
	BodyQue bodyque;
}
-spawn:(entity)ent;
-copyToBodyQue:(GameEntity)ent;
@end

@implementation World
-spawn:(entity)ent
{
	[self initWithEntity:ent];
	id (bodyque) = [[BodyQue alloc] init];
}

-copyToBodyQue:(GameEntity)ent
{
	[bodyque add:ent];
}
@end

void () worldspawn =
{
	world = [[World alloc] initWithEntity:@self];
};
