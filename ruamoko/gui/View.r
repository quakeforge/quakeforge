#include "gui/Size.h"
#include "gui/Point.h"
#include "gui/Rect.h"
#include "gui/View.h"

@implementation View

- (id) initWithComponents: (integer)x : (integer)y : (integer)w : (integer)h
{
	self = [self init];
	xpos = xabs = x;
	ypos = yabs = y;
	xlen = w;
	ylen = h;
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

- (id) canFocus: (integer) cf
{
	flags |= 1;
	return self;
}

- (integer) canFocus
{
	return flags & 1;
}

- (void) setBasePos: (integer) x y: (integer) y
{
	local Point point = {x, y};
	[self setBasePos:point];
}

- (void) setBasePos: (Point)pos
{
	xabs = xpos + pos.x;
	yabs = ypos + pos.y;
}

-(void) draw
{
}

- (integer) keyEvent:(integer)key unicode:(integer)unicode down:(integer)down
{
	return 0;
}

@end
