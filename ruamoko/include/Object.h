#ifndef __ruamoko_Object_h
#define __ruamoko_Object_h

#include <runtime.h>

@class Protocol;

@protocol Object
- (Class) class;
- (Class) superclass;
- (BOOL) isEqual: (id)anObject;
- (BOOL) isKindOfClass: (Class)aClass;
- (BOOL) isMemberOfClass: (Class)aClass;
#if 0
- (BOOL) isProxy;
#endif	// proxies
- (unsigned) hash;
- (id) self;
- (string) description;

- (id) performSelector: (SEL)aSelector;
- (id) performSelector: (SEL)aSelector
			withObject: (void *)anObject;
- (id) performSelector: (SEL)aSelector
			withObject: (void *)anObject
			withObject: (void *)anotherObject;
- (BOOL) respondsToSelector: (SEL)aSelector;
- (BOOL) conformsToProtocol: (Protocol *)aProtocol;

- (id) retain;
- (id) autorelease;
- (/*oneway*/ void) release;
- (unsigned) retainCount;
@end

@protocol Copying
/**
	Returns a copy of the receiver.
*/
- copy;
@end

/**
	The Ruamoko root class
*/
@interface Object <Object>
{
	Class		isa;
}

+ (id) alloc;
+ (id) new;
+ (Class) class;
+ (string) description;
+ (void) initialize;
+ (IMP) instanceMethodForSelector: (SEL)aSelector;
#if 0
+ (MethodSignature) instanceMethodSignatureForSelector: (SEL)aSelector;
#endif	// invocations
+ (BOOL) instancesRespondToSelector: (SEL)aSelector;
+ (BOOL) respondsToSelector: (SEL)aSelector;
+ (BOOL) conformsToProtocol: (Protocol *)aProtocol;
+ (BOOL) isKindOfClass: (Class)aClass;
+ (void) poseAsClass: (Class)aClass;
+ (Class) superclass;

+ (id) retain;
+ (id) autorelease;
+ (/*oneway*/ void) release;
+ (unsigned) retainCount;

- (id) init;
- (void) dealloc;
- (void) doesNotRecognizeSelector: (SEL)aSelector;
#if 0
- (void) forwardInvocation: (Invocation)anInvocation;
#endif	// invocations
#if 0
- (BOOL) isProxy;
#endif	// proxies
- (IMP) methodForSelector: (SEL)aSelector;
#if 0
- (MethodSignature) methodSignatureForSelector: (SEL)aSelector;
#endif	// invocations

- (id) copy;
- (id) mutableCopy;

@end

@interface Object (error)

- (void) error: (string)formatString, ...;

@end

#endif //__ruamoko_Object_h
