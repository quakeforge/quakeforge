#include "gui/Group.h"
#include "gui/Point.h"
#include "Array.h"

@implementation Group

- (id) initWithBounds: (Rect)bounds
{
	self = [super initWithBounds:bounds];
	views = [[Array alloc] init];
	return self;
}

- (void) dealloc
{
	[views dealloc];
}

- (View) addView: (View)aView
{
	[views addItem:aView];
}

- (void) moveTo: (integer)x y:(integer)y
{
	[self setBasePos: x y:y];
}

- (void) setBasePos: (Point) pos
{
	[super setBasePos:pos];
	local Point point = [[Point alloc] initWithComponents:xabs :yabs];
	[views makeObjectsPerformSelector:@selector (setBasePos:) withObject:point];
	[point dealloc];
}

- (void) draw
{
	[views makeObjectsPerformSelector:@selector (draw)];
}

@end
