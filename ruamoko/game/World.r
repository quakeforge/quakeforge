#include "GameEntity.h"
#include "World.h"

id		world;
int	deathmatch;

#define MAX_BODIES 8

@interface BodyQueue: Object
{
	entity	bodies[MAX_BODIES];
	int	head;
}

- (id) init;
- (void) addEntity: (GameEntity *)ent;

@end

@implementation BodyQueue

- (id) init
{
	local int	i;
	self = [super init];

	head = nil;

	for (i = 0; i < MAX_BODIES; i++) {
		local GameEntity*	ent = nil;
		ent = [[GameEntity alloc] init];
		bodies[i] = ent.ent;
	}
#if 0
	for (i = 0; i < MAX_BODIES; i++) {
		bodies[i] = [[[GameEntity alloc] init] ent];
	}
#endif
	return self;
}

- (void) addEntity: (GameEntity*)ent
{
	local entity	be = bodies[head++];
	local entity	e = [ent ent];

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
}
@end

@interface World: GameEntity
{
	id	bodyque;
}

- (void) spawn: (entity)ent;
- (void) copyToBodyQueue: (GameEntity *)ent;

@end

@implementation World

- (void) spawn: (entity)ent
{
	[self initWithEntity: ent];
	bodyque = [[BodyQueue alloc] init];
}

- (void) copyToBodyQueue: (GameEntity *)ent
{
	[bodyque addEntity: ent];
}
@end

void () worldspawn =
{
	world = [[World alloc] initWithEntity: @self];
};
