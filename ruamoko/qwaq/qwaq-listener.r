#include <Array.h>

#include "qwaq-listener.h"

@class Array;

@implementation Listener
-initWithResponder: (id) responder message: (SEL) message
{
	if (!(self = [super init])) {
		return nil;
	}
	self.responder = responder;
	self.message = message;
	imp = [responder methodForSelector: message];
	return self;
}

-(void)respond: (id) caller
{
	imp (responder, message, caller);
}

-(BOOL) matchResponder: (id) responder message: (SEL) message
{
	return self.responder == responder && self.message == message;
}
@end

@implementation ListenerGroup : Object
-init
{
	if (!(self = [super init])) {
		return nil;
	}
	listeners = [[Array array] retain];
	return self;
}

-addListener: (id) responder message: (SEL) message
{
	Listener   *listener = [[Listener alloc] initWithResponder: responder
													   message: message];
	if (listener) {
		[listeners addObject: listener];
	}
	return self;
}

-removeListener: (id) responder message: (SEL) message
{
	for (int i = [listeners count]; i-- > 0; ) {
		Listener   *l = [listeners objectAtIndex: i];
		if ([l matchResponder: responder message: message]) {
			[listeners removeObjectAtIndex: i];
		}
	}
	return self;
}

-(void)respond: (id) caller
{
	[listeners makeObjectsPerformSelector: @selector (respond:)
							   withObject: caller];
}
@end
