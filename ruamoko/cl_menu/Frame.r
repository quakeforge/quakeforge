#include "Frame.h"
#include "debug.h"

@implementation Frame : Object
- (id) initWithFile: (string) file duration: (float) time
{
	self = [super init];
	picture = [[QPic alloc] initName: file];
	duration = time;

	return self;
}

- (void) dealloc
{
	[picture release];
	[super dealloc];
}

- (Point) size
{
	return [[Point alloc] initWithComponents :[picture width] :[picture height]];
}

- (float) duration
{
	return duration;
}

- (void) draw: (integer) x :(integer) y
{
	[picture draw :x :y];
}
@end
