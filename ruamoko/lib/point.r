#include "point.h"

@implementation Point

-initAtX:(integer)_x Y:(integer)_y
{
	[super init];
	x = _x;
	y = _y;
	return self;
}

-initWithPoint:(Point)p
{
	[super init];
	x = p.x;
	y = p.y;
	return self;
}

-setTo:(Point)p
{
	x = p.x;
	y = p.y;
	return self;
}

-moveBy:(Point)p
{
	x += p.x;
	y += p.y;
	return self;
}
