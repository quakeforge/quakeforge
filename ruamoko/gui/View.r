#include <gui/Size.h>
#include <gui/Point.h>
#include <gui/Rect.h>
#include <gui/View.h>

@implementation View

- (id) initWithComponents: (int)x : (int)y : (int)w : (int)h
{
	self = [self init];
	xpos = xabs = x;
	ypos = yabs = y;
	xlen = w;
	ylen = h;
	parent = nil;
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

- (id) canFocus: (int) cf
{
	flags |= 1;
	return self;
}

- (int) canFocus
{
	return flags & 1;
}

- (Point) basePos
{
	return makePoint (xabs, yabs);
}

- (void) setBasePos: (int) x y: (int) y
{
	local Point point = {x, y};
	xabs = xpos + x;
	yabs = ypos + y;
}

- (void) setBasePosFromView: (View *) view
{
	Point pos = [view basePos];
	xabs = xpos + pos.x;
	yabs = ypos + pos.y;
}

-(void) draw
{
}

- (bool) keyEvent:(int)key unicode:(int)unicode down:(bool)down
{
	return false;
}

@end
