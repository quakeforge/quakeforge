#include "gui/Point.h"

@implementation Point

- (id) initWithComponents: (integer)_x : (integer)_y
{
	self = [self init];
	x = _x;
	y = _y;
	return self;
}

- (id) initWithPoint: (Point) aPoint
{
	self = [self init];

	if (!self || !aPoint)
		return NIL;

	x = [aPoint x];
	y = [aPoint y];

	return self;
}

- (id) copy
{
	local id	myCopy = [super copy];

	if (!myCopy)
		myCopy = [[self class] alloc];

	return [myCopy initWithComponents: x : y];
}

- (integer) x
{
	return x;
}

- (integer) y
{
	return y;
}

- (void) setPoint: (Point)aPoint
{
	x = [aPoint x];
	y = [aPoint y];
}

- (void) addPoint: (Point) aPoint
{
	x += [aPoint x];
	y += [aPoint y];
}

- (void) subtractPoint: (Point) aPoint
{
	x -= [aPoint x];
	y -= [aPoint y];
}

@end
