#include "Entity.h"

@implementation Entity

- (id) init
{
	return [self initWithEntity: spawn ()];
}

- (id) initWithEntity: (entity)e
{
	ent = e;
	ent.@this = self;
	return self;
}

- (void) free
{
	remove (ent);
	[super free];
}

- (entity) ent
{
	return self.ent;
}

@end
