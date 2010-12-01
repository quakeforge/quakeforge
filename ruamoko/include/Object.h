#ifndef __ruamoko_Object_h
#define __ruamoko_Object_h

/**
	Standard boolean type
*/
typedef enum {
	NO,			///< false
	YES,		///< true
} BOOL;

@extern void obj_error (id object, integer code, string fmt, ...);
@extern void obj_verror (id object, integer code, string fmt, @va_list args);
//obj_error_handler obj_set_error_handler (objc_error_handler func);
@extern IMP obj_msg_lookup (id receiver, SEL op);
@extern IMP obj_msg_lookup_super (Super class, SEL op);
@extern @param obj_msg_sendv (id receiver, SEL op, @va_list args);
@extern (void []) obj_malloc (integer size);
@extern (void []) obj_atomic_malloc (integer size);
@extern (void []) obj_valloc (integer size);
@extern (void []) obj_realloc (void [] mem, integer size);
@extern (void []) obj_calloc (integer nelem, integer size);
@extern void obj_free (void [] mem);
//(void []) obj_get_uninstalled_dtable (void);

@extern Class obj_get_class (string name);
@extern Class obj_lookup_class (string name);
//Class obj_next_class (void [][] enum_stage);

@extern string sel_get_name (SEL selector);
@extern string sel_get_type (SEL selector);
@extern SEL sel_get_uid (string name);
//@extern SEL sel_get_any_uid (string name);
//@extern SEL sel_get_any_typed_uid (string name);
//@extern SEL sel_get_typed_uid (string name, string type);
@extern SEL sel_register_name (string name);
//@extern SEL sel_register_typed_name (string name, string type);
@extern BOOL sel_is_mapped (SEL aSel);

@extern Method class_get_class_method (Class class, SEL aSel);
@extern Method class_get_instance_method (Class class, SEL aSel);
@extern Class class_pose_as (Class imposter, Class superclass);
@extern id class_create_instance (Class class);
@extern string class_get_class_name (Class class);
@extern integer class_get_instance_size (Class class);
@extern Class class_get_meta_class (Class class);
@extern Class class_get_super_class (Class class);
@extern integer class_get_version (Class class);
@extern BOOL class_is_class (Class class);
@extern BOOL class_is_meta_class (Class class);
@extern void class_set_version (Class class, integer version);
@extern (void []) class_get_gc_object_type (Class class);
@extern void class_ivar_set_gcinvisible (Class class, string ivarname, BOOL gcInvisible);

@extern IMP method_get_imp (Method method);
@extern IMP get_imp (Class class, SEL sel);

@extern id object_copy (id object);
@extern id object_dispose (id object);
@extern Class object_get_class (id object);
@extern string object_get_class_name (id object);
@extern Class object_get_meta_class (id object);
@extern Class object_get_super_class (id object);
@extern BOOL object_is_class (id object);
@extern BOOL object_is_instance (id object);
@extern BOOL object_is_meta_class (id object);

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
