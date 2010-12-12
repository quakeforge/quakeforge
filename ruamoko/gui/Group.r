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
	[views addObject:aView];
	return aView;
}

- (id) addViews: (Array)viewlist
{
	while ([viewlist count]) {
		[self addView: [viewlist objectAtIndex: 0]];
		[viewlist removeObjectAtIndex: 0];
	}
	return self;
}

- (void) moveTo: (integer)x y:(integer)y
{
	[self setBasePos: x y:y];
}

- (void) setBasePos: (integer) x y: (integer) y
{
	[super setBasePos: x y:y];
	local SEL sel = @selector (setBasePosFromView:);
	[views makeObjectsPerformSelector:sel withObject:self];
}

- (void) setBasePosFromView: (View) view
{
	[super setBasePosFromView:view];
	local SEL sel = @selector (setBasePosFromView:);
	[views makeObjectsPerformSelector:sel withObject:self];
}

- (void) draw
{
	[views makeObjectsPerformSelector:@selector (draw)];
}

@end
