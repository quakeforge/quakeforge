#ifndef __ruamoko_Object_h
#define __ruamoko_Object_h

typedef enum {
	NO,
	YES,
} BOOL;

@extern void (id object, integer code, string fmt, ...) obj_error;
@extern void (id object, integer code, string fmt, @va_list args) obj_verror;
//obj_error_handler (objc_error_handler func) obj_set_error_handler;
@extern IMP (id receiver, SEL op) obj_msg_lookup;
@extern IMP (Super class, SEL op) obj_msg_lookup_super;
@extern @param (id receiver, SEL op, @va_list args) obj_msg_sendv;
@extern (void []) (integer size) obj_malloc;
@extern (void []) (integer size) obj_atomic_malloc;
@extern (void []) (integer size) obj_valloc;
@extern (void []) (void [] mem, integer size) obj_realloc;
@extern (void []) (integer nelem, integer size) obj_calloc;
@extern void (void [] mem) obj_free;
//(void []) (void) obj_get_uninstalled_dtable;

@extern Class (string name) obj_get_class;
@extern Class (string name) obj_lookup_class;
//Class (void [][] enum_stage) obj_next_class;

@extern string (SEL selector) sel_get_name;
@extern string (SEL selector) sel_get_type;
@extern SEL (string name) sel_get_uid;
//@extern SEL (string name) sel_get_any_uid;
//@extern SEL (string name) sel_get_any_typed_uid;
//@extern SEL (string name, string type) sel_get_typed_uid;
@extern SEL (string name) sel_register_name;
//@extern SEL (string name, string type) sel_register_typed_name;
@extern BOOL (SEL aSel) sel_is_mapped;

@extern Method (Class class, SEL aSel) class_get_class_method;
@extern Method (Class class, SEL aSel) class_get_instance_method;
@extern Class (Class imposter, Class superclass) class_pose_as;
@extern id (Class class) class_create_instance;
@extern string (Class class) class_get_class_name;
@extern integer (Class class) class_get_instance_size;
@extern Class (Class class) class_get_meta_class;
@extern Class (Class class) class_get_super_class;
@extern integer (Class class) class_get_version;
@extern BOOL (Class class) class_is_class;
@extern BOOL (Class class) class_is_meta_class;
@extern void (Class class, integer version) class_set_version;
@extern (void []) (Class class) class_get_gc_object_type;
@extern void (Class class, string ivarname, BOOL gcInvisible) class_ivar_set_gcinvisible;

@extern IMP (Method method) method_get_imp;
@extern IMP (Class class, SEL sel) get_imp;

@extern id (id object) object_copy;
@extern id (id object) object_dispose;
@extern Class (id object) object_get_class;
@extern string (id object) object_get_class_name;
@extern Class (id object) object_get_meta_class;
@extern Class (id object) object_get_super_class;
@extern BOOL (id object) object_is_class;
@extern BOOL (id object) object_is_instance;
@extern BOOL (id object) object_is_meta_class;

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
			withObject: (id)anObject;
- (id) performSelector: (SEL)aSelector
			withObject: (id)anObject
			withObject: (id)anotherObject;
- (BOOL) respondsToSelector: (SEL)aSelector;
- (BOOL) conformsToProtocol: (Protocol)aProtocol;

- (id) retain;
- (id) autorelease;
- (/*oneway*/ void) release;
- (unsigned) retainCount;
@end

@protocol Copying
- copy;
@end

@protocol MutableCopying
- mutableCopy;
@end

@interface Object <Object>
{
	Class		isa;
	unsigned	retainCount;
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
+ (BOOL) conformsToProtocol: (Protocol)aProtocol;
+ (BOOL) isKindOfClass: (Class)aClass;
+ (void) poseAsClass: (Class)aClass;
+ (Class) superclass;

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
