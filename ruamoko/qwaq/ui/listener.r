#include <Array.h>

#include "ui/listener.h"
#include "ui/curses.h"

@class Array;

@implementation Listener
+(Listener *)listenerWithResponder:(id)responder :(SEL)message
{
	return [[[self alloc] initWithResponder:responder :message] autorelease];
}

-initWithResponder: (id) responder :(SEL)message
{
	if (!(self = [super init])) {
		return nil;
	}
	self.responder = responder;
	self.message = message;
	imp = [responder methodForSelector: message];
	if (!imp) {
		[self error:"method not found: %s", sel_get_name (message)];
	}
	return self;
}

-(void)respond: (void *) caller_data
{
	imp (responder, message, caller_data);
}

-(void)respond: (void *) caller_data withObject:(void *)anObject
{
	imp (responder, message, caller_data, anObject);
}

-(BOOL) matchResponder: (id) responder :(SEL)message
{
	return self.responder == responder && self.message == message;
}
@end

@implementation ListenerGroup : Object
+(ListenerGroup *)listener
{
	return [[[self alloc] init] autorelease];
}

-init
{
	if (!(self = [super init])) {
		return nil;
	}
	listeners = [[Array array] retain];
	return self;
}

-(void)dealloc
{
	[listeners release];
	[super dealloc];
}

-addListener: (id) responder :(SEL)message
{
	Listener   *listener = [Listener listenerWithResponder:responder :message];
	if (listener) {
		[listeners addObject:listener];
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

-(void)respond: (void *) caller_data withObject:(void *)anObject
{
	[listeners makeObjectsPerformSelector: @selector (respond:)
							   withObject: caller_data
							   withObject: anObject];
}
@end
