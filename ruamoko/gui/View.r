#include "gui/Size.h"
#include "gui/Point.h"
#include "gui/Rect.h"
#include "gui/View.h"

@implementation View

- (id) initWithComponents: (integer)x : (integer)y : (integer)w : (integer)h
{
	xpos = xabs = x;
	ypos = yabs = y;
	xlen = w;
	ylen = y;
	parent = NIL;
	return self;
}

- (id) initWithOrigin: (Point)anOrigin size: (Size)aSize
{
	return [self initWithComponents:anOrigin.x :anOrigin.y
								   :aSize.width :aSize.height];
}

- (id) initWithBounds: (Rect)aRect
{
	return [self initWithOrigin:aRect.origin size:aRect.size];
}

- (void) setBasePos: (integer) x y: (integer) y
{
	local Point point = [[Point alloc] initWithComponents:x :y];
	[self setBasePos:point];
	[point dealloc];
}

- (void) setBasePos: (Point)pos
{
	xabs = xpos + pos.x;
	yabs = ypos + pos.y;
}

-(void) draw
{
}

@end
