#include "Object.h"
#include "AutoreleasePool.h"

void (obj_module_t [] msg) __obj_exec_class = #0;
void (id object, integer code, string fmt, ...) obj_error = #0;
void (id object, integer code, string fmt, @va_list args) obj_verror = #0;
//obj_error_handler (objc_error_handler func) obj_set_error_handler = #0;
IMP (id receiver, SEL op) obj_msg_lookup = #0;
IMP (Super class, SEL op) obj_msg_lookup_super = #0;
id (id receiver, SEL op, ...) obj_msgSend = #0;
id (Super class, SEL op, ...) obj_msgSend_super = #0;
//retval_t (id receiver, SEL op, @va_list args) obj_msg_sendv = #0;
(void []) (integer size) obj_malloc = #0;
(void []) (integer size) obj_atomic_malloc = #0;
(void []) (integer size) obj_valloc = #0;
(void []) (void [] mem, integer size) obj_realloc = #0;
(void []) (integer nelem, integer size) obj_calloc = #0;
void (void [] mem) obj_free = #0;
//(void []) (void) obj_get_uninstalled_dtable = #0;

Class (string name) obj_get_class = #0;
Class (string name) obj_lookup_class = #0;
//Class (void [][] enum_stage) obj_next_class = #0;

string (SEL selector) sel_get_name = #0;
string (SEL selector) sel_get_type = #0;
SEL (string name) sel_get_uid = #0;
//SEL (string name) sel_get_any_uid = #0;
//SEL (string name) sel_get_any_typed_uid = #0;
//SEL (string name, string type) sel_get_typed_uid = #0;
SEL (string name) sel_register_name = #0;
//SEL (string name, string type) sel_register_typed_name = #0;
BOOL (SEL aSel) sel_is_mapped = #0;

Method (Class class, SEL aSel) class_get_class_method = #0;
Method (Class class, SEL aSel) class_get_instance_method = #0;
Class (Class imposter, Class superclass) class_pose_as = #0;
id (Class class) class_create_instance = #0;
string (Class class) class_get_class_name = #0;
integer (Class class) class_get_instance_size = #0;
Class (Class class) class_get_meta_class = #0;
Class (Class class) class_get_super_class = #0;
integer (Class class) class_get_version = #0;
BOOL (Class class) class_is_class = #0;
BOOL (Class class) class_is_meta_class = #0;
void (Class class, integer version) class_set_version = #0;
(void []) (Class class) class_get_gc_object_type = #0;
void (Class class, string ivarname, BOOL gcInvisible) class_ivar_set_gcinvisible = #0;

IMP (Method method) method_get_imp = #0;
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

@implementation Object

+ (void) initialize
{
#if 0
	allocDebug = localinfo ("AllocDebug");
	autoreleaseClass = [AutoreleasePool class];
	autoreleaseSelector = @selector(addObject:);
	autoreleaseIMP = [autoreleaseClass methodForSelector: autoreleaseSelector];
#endif
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

+ (BOOL) conformsToProtocol: (Protocol)aProtocol = #0;

+ (BOOL) instancesRespondToSelector: (SEL)aSelector
{
	return class_get_instance_method (self, aSelector) != NIL;
}

+ (BOOL) respondsToSelector: (SEL)aSelector
{
	return (class_get_class_method (self, aSelector) != NIL);
}

/*
	Returns a pointer to the function providing the instance method that is
	used to	respond to messages with the selector held in aSelector.
*/
+ (IMP) instanceMethodForSelector: (SEL)aSelector
{
	if (!aSelector)
		return NIL;

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
	retainCount = 1;

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

- (unsigned) hash = #0;	// can't cast pointer to integer

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
	return (class_get_instance_method ([self class], aSelector) != NIL);
}

- (BOOL) conformsToProtocol: (Protocol)aProtocol
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
		return NIL;

	return method_get_imp (object_is_instance (self)
						   ? class_get_instance_method (myClass, aSelector)
						   : class_get_class_method (myClass, aSelector));
}

//+(struct objc_method_description *)descriptionForInstanceMethod:(SEL)aSel = #0;
//-(struct objc_method_description *)descriptionForMethod:(SEL)aSel = #0;

- (id) performSelector: (SEL)aSelector
{
	local IMP msg;

	if (!aSelector || !(msg = obj_msg_lookup (self, aSelector)))
		[self error: "invalid selector passed to %s: %s",
					object_get_class_name (self),
					sel_get_name (aSelector)];

	return msg (self, aSelector);
}

- (id) performSelector: (SEL)aSelector withObject: (id)anObject
{
	local IMP msg;

	if (!aSelector || !(msg = obj_msg_lookup (self, aSelector)))
		[self error: "invalid selector passed to %s: %s",
					object_get_class_name (self),
					sel_get_name (aSelector)];

	return msg (self, aSelector, anObject);
}

- (id) performSelector: (SEL)aSelector
			withObject: (id)anObject
			withObject: (id)anotherObject
{
	local IMP msg;

	if (!aSelector || !(msg = obj_msg_lookup (self, aSelector)))
		[self error: "invalid selector passed to %s",
					sel_get_name (_cmd)];

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
	retainCount = [self retainCount] + 1;
	return self;
}

- (/*oneway*/ void) release
{
	if ([self retainCount] == 1)	// don't let retain count actually reach zero
		[self dealloc];
	else
		retainCount--;
}

- (id) autorelease
{
	
}

- (unsigned) retainCount
{
	return retainCount;
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
