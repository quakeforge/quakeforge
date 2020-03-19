#ifndef __qwaq_listener_h
#define __qwaq_listener_h

#include <Object.h>

@class Array;

@interface Listener : Object
{
	id          responder;
	SEL         message;
	IMP         imp;
}
-initWithResponder: (id) responder message: (SEL) message;
-(void) respond: (id) caller;
-(BOOL) matchResponder: (id) responder message: (SEL) message;
@end

@interface ListenerGroup : Object
{
	Array      *listeners;
}
-init;
-addListener: (id) responder message: (SEL) message;
-removeListener: (id) responder message: (SEL) message;
-(void) respond: (id) caller;
@end

#endif//__qwaq_listener_h
