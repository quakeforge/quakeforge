#include "gui/Group.h"
#include "gui/Point.h"
#include "Array.h"

@implementation Group

- (id) init
{
	self = [super init];
	views = [[Array alloc] init];
	return self;
}

- (void) dealloc
{
	[views release];
	[super dealloc];
}

- (View) addView: (View)aView
{
	[views addItem:aView];
	return aView;
}

- (id) addViews: (Array)viewlist
{
	while ([viewlist count]) {
		[self addView:[viewlist getItemAt:0]];
		[viewlist removeItemAt:0];
	}
	return self;
}

- (void) moveTo: (integer)x y:(integer)y
{
	[self setBasePos: x y:y];
}

- (void) setBasePos: (Point) pos
{
	[super setBasePos:pos];
	local Point point = {xabs, yabs};
	[views makeObjectsPerformSelector:@selector (setBasePos:) withObject:point];
}

- (void) draw
{
	[views makeObjectsPerformSelector:@selector (draw)];
}

@end
