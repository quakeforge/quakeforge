#include <string.h>
#include <Array.h>

#include "fsm.h"

@implementation FSMAction
+ (FSMAction *)actionWithTarget:(id)target selector:(SEL)selector
{
	return [[[self alloc] initWithTarget:target selector:selector] autorelease];
}

- (id)initWithTarget:(id)target selector:(SEL)selector
{
	if (!(self = [super init])) {
		return self;
	}
	// Weak reference: controller owns FSM, so don't retain target
	self.target = target;
	self.selector = selector;
	[self bind];
	return self;
}

-(void)awakeFromArchive
{
	[super awakeFromArchive];
	[self bind];
}

- (void)bind
{
	if (target && selector) {
		imp = [target methodForSelector:selector];
	}
}

- (void)invoke
{
	if (target && imp) {
		imp (target, selector);
	}
}

- (bool)test
{
	return @bitcast(bool (*)(id, SEL), imp) (target, selector);
}

- (void)invokeWithObject:(id)arg
{
	if (target && imp) {
		@bitcast(void (*)(id, SEL, id),imp)(target, selector, arg);
	}
}
@end

@implementation FSMEvent
+ (FSMEvent *)eventWithName:(string)aName targetState:(FSMState *)target
{
	return [[[self alloc] initWithName:aName targetState:target] autorelease];
}

- (id)initWithName:(string)aName targetState:(FSMState *)target
{
	if (!(self = [super init])) {
		return self;
	}
	name = aName;
	targetState = target; // Weak/unretained: FSM owns all states in its states array
	return self;
}

- (bool)evaluate
{
	return [checkCondition test];
	return false;
}
@end

@implementation FSMState
+ (FSMState *)stateWithName:(string)aName
{
	return [[[self alloc] initWithName:aName] autorelease];
}

- (id)initWithName:(string)aName
{
	if (!(self = [super init])) {
		return self;
	}
	name = aName;
	events = [[Array array] retain];
	return self;
}

- (void)addEvent:(FSMEvent *)event
{
	[events addObject:event];
}

- (void)dealloc
{
	[onEnter release];
	[onUpdate release];
	[onExit release];
	[events release];
	[super dealloc];
}
@end

@implementation FSM
- (id)init
{
	if (!(self = [super init])) {
		return self;
	}
	states = [[Array array] retain];
	globalEvents = [[Array array] retain];
	return self;
}

- (void)addState:(FSMState *)state
{
	[states addObject:state];
	if (!initialState) {
		initialState = state;
	}
}

- (void)addGlobalEvent:(FSMEvent *)event
{
	[globalEvents addObject:event];
}

- (void)start
{
	if (initialState) {
		[self transitionTo:initialState viaEvent:nil];
	}
}

- (void)transitionTo:(FSMState *)nextState viaEvent:(FSMEvent *)event
{
	if (!nextState) return;

	if (currentState) {
		[currentState.onExit invoke];
	}

	lastState = currentState;
	currentState = nextState;
	timeInState = 0;

	if (event) {
		[event.onEvent invoke];
	}

	[currentState.onEnter invoke];
}

- (bool)triggerEventNamed:(string)eventName
{
	// 1. Check current state's outgoing events
	if (currentState) {
		for (uint i = 0; i < [currentState.events count]; i++) {
			FSMEvent *ev = [currentState.events objectAtIndex:i];
			if (ev.name == eventName) {
				[self transitionTo:ev.targetState viaEvent:ev];
				return true;
			}
		}
	}
	// 2. Check global events
	for (uint i = 0; i < [globalEvents count]; i++) {
		FSMEvent *ev = [globalEvents objectAtIndex:i];
		if (ev.name == eventName) {
			[self transitionTo:ev.targetState viaEvent:ev];
			return true;
		}
	}
	return false;
}

- (void)update:(float)dt
{
	if (!currentState) return;
	timeInState += dt;

	// Evaluate automatic condition-based events
	for (uint i = 0; i < [currentState.events count]; i++) {
		FSMEvent *ev = [currentState.events objectAtIndex:i];
		if ([ev evaluate]) {
			[self transitionTo:ev.targetState viaEvent:ev];
			return;
		}
	}

	[currentState.onUpdate invoke];
}
@end
