#include "ruamoko/qwaq/ui/group.h"
#include "ruamoko/qwaq/ui/proxyview.h"

@implementation ProxyView
+(ProxyView *)withView:(View *)view
{
	return [[[self alloc] initWithView:view] autorelease];
}

- (void) forward: (SEL) sel : (@va_list) args
{
	if (!view) {
		@return nil;
	}
	@return obj_msg_sendv (view, sel, args);
}

-initWithView:(View *) view
{
	if (!(self = [super init])) {
		return nil;
	}
	self.view = [view retain];
	return self;
}

-setOwner:(Group *)owner
{
	self.owner = owner;
	return [view setOwner:owner];
}

-setView:(View *) view
{
	int         state = [self.view state];

	if (state & sfInFocus) {
		[self.view loseFocus];
	}
	[self.view hide];
	[self.view setContext:nil];
	[self.view setOwner:nil];

	[view retain];
	[self.view release];
	self.view = view;

	[view setOwner:owner];
	[view setContext:[owner context]];
	if (state & sfDrawn) {
		[view draw];
	}
	if (state & sfInFocus) {
		[view takeFocus];
	}
	return self;
}
@end
