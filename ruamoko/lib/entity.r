#include "entity.h"

entity () spawn = #0;
void (entity e) remove = #0;

@implementation Entity
-init
{
	return [self initWithEntity:[self new]];
}

-initWithEntity:(entity)e
{
	self.ent = e;
	e.@this = self;
	return self;
}

-free
{
	remove (self.ent);
	return self;
}

-new
{
	return spawn ();
}

-ent
{
	return self.ent;
}
@end
