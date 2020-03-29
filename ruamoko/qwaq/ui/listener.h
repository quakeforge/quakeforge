#ifndef __qwaq_ui_listener_h
#define __qwaq_ui_listener_h

#include <Object.h>

@class Array;

@interface Listener : Object
{
	id          responder;
	SEL         message;
	IMP         imp;
}
-initWithResponder: (id) responder :(SEL)message;
-(void) respond: (void *) caller_data;
-(void) respond: (void *) caller_data withObject:(void *)anObject;
-(BOOL) matchResponder: (id) responder :(SEL)message;
@end

@interface ListenerGroup : Object
{
	Array      *listeners;
}
-init;
-addListener: (id) responder :(SEL)message;
-removeListener: (id) responder :(SEL)message;
-(void) respond: (void *) caller_data;
-(void) respond: (void *) caller_data withObject:(void *)anObject;
@end

#endif//__qwaq_ui_listener_h
