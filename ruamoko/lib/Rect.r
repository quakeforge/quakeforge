#include "Object.h"
#include "Point.h"
#include "Size.h"
#include "Rect.h"

@implementation Rect

- (id) initWithComponents: (integer)x : (integer)y : (integer)w : (integer)h
{
	id (self) = [super init];
	id (origin) = [[Size alloc] initWithComponents: x : y];
	id (size) = [[Size alloc] initWithComponents: w : h];
	return self;
}

- (id) initWithOrigin: (Point)anOrigin size: (Size)aSize
{
	id (self) = [super init];

	if (!self || !anOrigin || !aSize)
		return NIL;

	id (origin) = [anOrigin copy];
	id (size) = [aSize copy];

	return self;
}

- (id) initWithRect: (Rect)aRect
{
	id (self) = [super init];

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

	id (origin) = [aPoint copy];
}

- (void) setSize: (Size)aSize
{
	if (!aSize)
		return;

	if (size)
		[size free];

	id (size) = [aSize copy];
}

- (void) setRect: (Rect)aRect
{
	[self setOrigin: [aRect origin]];
	[self setSize: [aRect size]];
}

@end
