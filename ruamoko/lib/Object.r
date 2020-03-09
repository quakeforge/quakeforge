#include <Object.h>
#include <AutoreleasePool.h>

static void link__obj_forward (void)
{
	__obj_forward (nil, nil);
}

void *PR_FindGlobal (string name) = #0;	//FIXME where?

void __obj_exec_class (struct obj_module *msg) = #0;
BOOL __obj_responds_to(id obj, SEL sel) = #0;
void (id object, int code, string fmt, ...) obj_error = #0;
void (id object, int code, string fmt, @va_list args) obj_verror = #0;
//obj_error_handler (objc_error_handler func) obj_set_error_handler = #0;
IMP (id receiver, SEL op) obj_msg_lookup = #0;
IMP (Super class, SEL op) obj_msg_lookup_super = #0;
id (id receiver, SEL op, ...) obj_msgSend = #0;
id obj_msgSend_super (Super *class, SEL op, ...) = #0;
@param (id receiver, SEL op, @va_list args) obj_msg_sendv = #0;
int obj_decrement_retaincount (id object) = #0;
int obj_increment_retaincount (id object) = #0;
int obj_get_retaincount (id object) = #0;
void *obj_malloc (int size) = #0;
void *obj_atomic_malloc (int size) = #0;
void *obj_valloc (int size) = #0;
void *obj_realloc (void *mem, int size) = #0;
void *obj_calloc (int nelem, int size) = #0;
void obj_free (void *mem) = #0;
//(void *) (void) obj_get_uninstalled_dtable = #0;

Class (string name) obj_get_class = #0;
Class (string name) obj_lookup_class = #0;
//Class (void **enum_stage) obj_next_class = #0;

string (SEL selector) sel_get_name = #0;
string (SEL selector) sel_get_type = #0;
SEL (string name) sel_get_uid = #0;
//SEL (string name) sel_get_any_uid = #0;
//SEL (string name) sel_get_any_typed_uid = #0;
//SEL (string name, string type) sel_get_typed_uid = #0;
SEL (string name) sel_register_name = #0;
//SEL (string name, string type) sel_register_typed_name = #0;
BOOL (SEL aSel) sel_is_mapped = #0;

Method *class_get_class_method (Class class, SEL aSel) = #0;
Method *class_get_instance_method (Class class, SEL aSel) = #0;
Class (Class imposter, Class superclass) class_pose_as = #0;
id (Class class) class_create_instance = #0;
string (Class class) class_get_class_name = #0;
int (Class class) class_get_instance_size = #0;
Class (Class class) class_get_meta_class = #0;
Class (Class class) class_get_super_class = #0;
int (Class class) class_get_version = #0;
BOOL (Class class) class_is_class = #0;
BOOL (Class class) class_is_meta_class = #0;
void (Class class, int version) class_set_version = #0;
void *class_get_gc_object_type (Class class) = #0;
void (Class class, string ivarname, BOOL gcInvisible) class_ivar_set_gcinvisible = #0;

IMP method_get_imp (Method *method) = #0;
IMP (Class class, SEL sel) get_imp = #0;

id (id object) object_copy = #0;
id (id object) object_dispose = #0;
Class (id object) object_get_class = #0;
string (id object) object_get_class_name = #0;
Class (id object) object_get_meta_class = #0;
Class (id object) object_get_super_class = #0;
BOOL (id object) object_is_class = #0;
BOOL (id object) object_is_instance = #0;
BOOL (id object) object_is_meta_class = #0;

@implementation Object (error)

- (void) error: (string)aString, ... = #0;

@end

@static BOOL allocDebug;
@static Class autoreleaseClass;
@static SEL autoreleaseSelector;
@static IMP autoreleaseIMP;

@implementation Object

+ (void) initialize
{
	//allocDebug = localinfo ("AllocDebug");
	autoreleaseClass = [AutoreleasePool class];
	autoreleaseSelector = @selector(addObject:);
	autoreleaseIMP = [autoreleaseClass methodForSelector: autoreleaseSelector];
	return;
}

+ (id) alloc
{
	return class_create_instance (self);
}

+ (Class) class
{
	return self;
}

+ (Class) superclass
{
	return class_get_super_class (self);
}

+ (string) description
{
	return class_get_class_name (self);
}

+ (id) new
{
	return [[self alloc] init];
}

+ (BOOL) isKindOfClass: (Class)aClass
{
	if (aClass == [Object class])
		return YES;
	return NO;
}

+ (BOOL) conformsToProtocol: (Protocol *)aProtocol = #0;

+ (BOOL) instancesRespondToSelector: (SEL)aSelector
{
	return class_get_instance_method (self, aSelector) != nil;
}

+ (BOOL) respondsToSelector: (SEL)aSelector
{
	return (class_get_class_method (self, aSelector) != nil);
}

/*
	Returns a pointer to the function providing the instance method that is
	used to	respond to messages with the selector held in aSelector.
*/
+ (IMP) instanceMethodForSelector: (SEL)aSelector
{
	if (!aSelector)
		return nil;

	return method_get_imp (class_get_instance_method (self, aSelector));
}

+ (void) poseAsClass: (Class)aClass
{
	class_pose_as (self, aClass);
}

/*
	INSTANCE METHODS
*/

- (id) init
{
	obj_increment_retaincount (self);
	return self;
}

- (void) dealloc
{
	object_dispose (self);
}

- (Class) class
{
	return object_get_class (self);
}

- (Class) superclass
{
	return object_get_super_class (self);
}

- (string) description
{
	return object_get_class_name (self);
}

- (id) self
{
	return self;
}

- (unsigned) hash = #0;	// can't cast pointer to int

- (BOOL) isEqual: (id)anObject
{
	return self == anObject;
}

- (BOOL) isKindOfClass: (Class)aClass
{
	local Class	class;

	for (class = [self class]; class; class = class_get_super_class (class))
		if (class == aClass)
			return YES;
	return NO;
}

- (BOOL) isMemberOfClass: (Class)aClass
{
	return ([self class] == aClass);
}

- (BOOL) respondsToSelector: (SEL)aSelector
{
	return (class_get_instance_method ([self class], aSelector) != nil);
}

- (BOOL) conformsToProtocol: (Protocol *)aProtocol
{
	return [[self class] conformsToProtocol: aProtocol];
}

/*
	Returns a pointer to the function providing the method used to respond to
	aSelector. If "self" is an instance, an instance method is returned. If
	it's a class, a class method is returned.
*/
- (IMP) methodForSelector: (SEL)aSelector
{
	local Class	myClass = [self class];

	if (!aSelector)
		return nil;

	return method_get_imp (object_is_instance (self)
						   ? class_get_instance_method (myClass, aSelector)
						   : class_get_class_method (myClass, aSelector));
}

//+(struct objc_method_description *)descriptionForInstanceMethod:(SEL)aSel = #0;
//-(struct objc_method_description *)descriptionForMethod:(SEL)aSel = #0;

- (id) performSelector: (SEL)aSelector
{
	local IMP msg = nil;		// FIXME teach qfcc about noreturn

	if (!aSelector || !(msg = obj_msg_lookup (self, aSelector)))
		[self error: "invalid selector passed to %s: %s",
					object_get_class_name (self),
					sel_get_name (aSelector)];

	return msg (self, aSelector);
}

- (id) performSelector: (SEL)aSelector withObject: (void *)anObject
{
	local IMP msg = nil;		// FIXME teach qfcc about noreturn

	if (!aSelector || !(msg = obj_msg_lookup (self, aSelector)))
		[self error: "invalid selector passed to %s: %s",
					object_get_class_name (self),
					sel_get_name (aSelector)];

	return msg (self, aSelector, anObject);
}

- (id) performSelector: (SEL)aSelector
			withObject: (void *)anObject
			withObject: (void *)anotherObject
{
	local IMP msg;

	if (!aSelector || !(msg = obj_msg_lookup (self, aSelector))) {
		[self error: "invalid selector passed to %s",
					sel_get_name (_cmd)];
		return nil;		//FIXME teach qfcc about noreturn
	}

	return msg (self, aSelector, anObject, anotherObject);
}

- (void) doesNotRecognizeSelector: (SEL)aSelector
{
	[self error: "%s does not recognize %s",
		object_get_class_name (self),
		sel_get_name (aSelector)];
}

/*
	MEMORY MANAGEMENT

	These methods handle the manual garbage collection scheme.
*/
- (id) retain
{
	obj_increment_retaincount (self);
	return self;
}

- (/*oneway*/ void) release
{
	int         rc;

	rc = obj_decrement_retaincount (self);
	if (rc < 0)
		obj_error (self, 0, "retain count went negative");
	if (rc == 0)
		[self dealloc];
}

- (id) autorelease
{
	autoreleaseIMP (autoreleaseClass, autoreleaseSelector, self);
	return self;
}

- (unsigned) retainCount
{
	return obj_get_retaincount (self);
}

/*	The class implementations of autorelease, retain and release are dummy
	methods with no effect. They allow class objects to be stored in
	containers that send retain and release messages. Thus, the class
	implementation of retainCount always returns the maximum unsigned value.
*/
+ (id) autorelease
{
	return self;
}

+ (id) retain
{
	return self;
}

+ (/*oneway*/ void) release
{
}

+ (unsigned) retainCount
{
	return UINT_MAX;
}

/*
	CONVENIENCE METHODS

	These methods exist only so that certain methods always work.
*/

- (id) copy
{
	return self;
}

- (id) mutableCopy
{
	return self;
}

@end
