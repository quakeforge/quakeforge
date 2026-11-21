#ifndef __fsm_h
#define __fsm_h

#include <Object.h>

@class Array;
@class FSMState;

@interface FSMAction : Object
{
@public
	id target;
	SEL selector;
	IMP imp;
}
+ (FSMAction *)actionWithTarget:(id)aTarget selector:(SEL)aSelector;
- (id)initWithTarget:(id)aTarget selector:(SEL)aSelector;
- (void)bind;
- (void)invoke;
- (void)invoke:(id)sender, ...;
- (bool)test;
- (void)invokeWithObject:(id)arg;
@end

@interface FSMEvent : Object
{
@public
	string name;
	FSMState *targetState;
	FSMAction *onEvent;			// Optional callback fired during transition
	FSMAction *checkCondition;	// Optional BOOL-returning condition method
}
+ (FSMEvent *)eventWithName:(string)aName targetState:(FSMState *)target;
- (id)initWithName:(string)aName targetState:(FSMState *)target;
- (bool)evaluate;
@end

@interface FSMState : Object
{
@public
	string name;
	FSMAction *onEnter;
	FSMAction *onUpdate;
	FSMAction *onExit;
	Array *events;
}
+ (FSMState *)stateWithName:(string)aName;
- (id)initWithName:(string)aName;
- (void)addEvent:(FSMEvent *)event;
@end

@interface FSM : Object
{
@public
	Array *states;
	Array *globalEvents;
	FSMState *initialState;
	FSMState *currentState;
	FSMState *lastState;
	float timeInState;
}
- (void)addState:(FSMState *)state;
- (void)addGlobalEvent:(FSMEvent *)event;
- (void)start;
- (void)transitionTo:(FSMState *)nextState viaEvent:(FSMEvent *)event;
- (bool)triggerEventNamed:(string)eventName;
- (void)update:(float)dt;
@end

#endif//__fsm_h
