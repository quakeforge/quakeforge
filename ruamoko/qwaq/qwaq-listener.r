#include <Array.h>

#include "qwaq-listener.h"

@class Array;

@implementation Listener
-initWithResponder: (id) responder :(SEL)message
{
	if (!(self = [super init])) {
		return nil;
	}
	self.responder = responder;
	self.message = message;
	imp = [responder methodForSelector: message];
	return self;
}

-(void)respond: (void *) caller_data
{
	imp (responder, message, caller_data);
}

-(BOOL) matchResponder: (id) responder :(SEL)message
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

-addListener: (id) responder :(SEL)message
{
	Listener   *listener = [[Listener alloc] initWithResponder: responder
															  : message];
	if (listener) {
		[listeners addObject: listener];
	}
	return self;
}

-removeListener: (id) responder :(SEL)message
{
	for (int i = [listeners count]; i-- > 0; ) {
		Listener   *l = [listeners objectAtIndex: i];
		if ([l matchResponder: responder : message]) {
			[listeners removeObjectAtIndex: i];
		}
	}
	return self;
}

-(void)respond: (void *) caller_data
{
	[listeners makeObjectsPerformSelector: @selector (respond:)
							   withObject: caller_data];
}
@end
