#include "Object.h"
#include "Point.h"
#include "Size.h"
#include "Rect.h"

@implementation Rect

- (id) initWithComponents: (integer)x : (integer)y : (integer)w : (integer)h
{
	self = [super init];
	origin = [[Size alloc] initWithComponents: x : y];
	size = [[Size alloc] initWithComponents: w : h];
	return self;
}

- (id) initWithOrigin: (Point)anOrigin size: (Size)aSize
{
	self = [super init];

	if (!self || !anOrigin || !aSize)
		return NIL;

	origin = [anOrigin copy];
	size = [aSize copy];

	return self;
}

- (id) initWithRect: (Rect)aRect
{
	self = [super init];

	if (!self || !aRect)
		return NIL;

	[self setRect: aRect];

	return self;
}

- (id) copy
{
	local id	myCopy = [super copy];

	if (!myCopy)
		myCopy = [[self class] alloc];

	return [myCopy initWithOrigin: origin size: size];
}

- (Point) origin
{
	return origin;
}

- (Size) size
{
	return size;
}

- (void) setOrigin: (Point)aPoint
{
	if (!aPoint)
		return;

	if (origin)
		[origin free];

	origin = [aPoint copy];
}

- (void) setSize: (Size)aSize
{
	if (!aSize)
		return;

	if (size)
		[size free];

	size = [aSize copy];
}

- (void) setRect: (Rect)aRect
{
	[self setOrigin: [aRect origin]];
	[self setSize: [aRect size]];
}

@end
