#include <string.h>
#include "virtinput.h"

@implementation VirtualInput
-initWithButton:(string)name ctx:(imui_ctx_t)ctx
{
	if (!(self = [super initWithName:name ctx:ctx])) {
		return nil;
	}
	description = str_hold (IN_GetButtonDescription (name));
	return self;
}

-initWithAxis:(string)name ctx:(imui_ctx_t)ctx
{
	if (!(self = [super initWithName:name ctx:ctx])) {
		return nil;
	}
	description = str_hold (IN_GetAxisDescription (name));
	return self;
}

+(VirtualInput *) button:(string)name ctx:(imui_ctx_t)ctx
{
	return [[[self alloc] initWithButton:name ctx:ctx] autorelease];
}

+(VirtualInput *) axis:(string)name ctx:(imui_ctx_t)ctx
{
	return [[[self alloc] initWithAxis:name ctx:ctx] autorelease];
}

-draw
{
	[super draw];
	return self;
}
@end
