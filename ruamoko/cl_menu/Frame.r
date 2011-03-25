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

- (Size) size
{
	return makeSize ([picture width], [picture height]);
}

- (float) duration
{
	return duration;
}

- (void) draw: (int) x :(int) y
{
	[picture draw :x :y];
}
@end
