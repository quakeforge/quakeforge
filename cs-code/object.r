typedef enum {
	NO,
	YES,
} BOOL;

void (obj_module_t [] module) __obj_exec_class = #0;
void (id object, integer code, string fmt, ...) obj_error = #0;
void (id object, integer code, string fmt, ...) obj_verror = #0;//FIXME not ...
//obj_error_handler (objc_error_handler func) obj_set_error_handler = #0;
IMP (id receiver, SEL op) obj_msg_lookup = #0;
IMP (id receiver, SEL op) obj_msg_lookup_super = #0;
id (id receiver, SEL op, ...) obj_msgSend = #0;
id (id receiver, SEL op, ...) obj_msgSend_super = #0;
//retval_t (id receiver, SEL op, arglist_t) obj_msg_sendv = #0;
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
SEL (string name) sel_get_any_uid = #0;
SEL (string name) sel_get_any_typed_uid = #0;
SEL (string name, string type) sel_get_typed_uid = #0;
SEL (string name) sel_register_name = #0;
SEL (string name, string type) sel_register_typed_name = #0;
BOOL (SEL aSel) sel_is_mapped = #0;

Method (Class class, SEL aSel) class_get_class_method = 0;
Method (Class class, SEL aSel) class_get_instance_method = 0;
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

@interface Object
{
	Class	isa;
}

+initialize;
-init;

+new;
+alloc;
-free;
-copy;
-shallowCopy;
-deepen;
-deepCopy;

-(Class)class;
-(Class)superClass;
-(Class)metaClass;
-(string)name;

-self;
-(integer)hash;
-(BOOL)isEqual:anObject;
-(integer)compare:anotherObject;

-(BOOL)isMetaClass;
-(BOOL)isClass;
-(BOOL)isInstance;

-(BOOL)isKindOf:(Class)aClassObject;
-(BOOL)isMemberOf:(Class)aClassObject;
-(BOOL)isKindOfClassNamed:(string)aClassName;
-(BOOL)isMemberOfClassNamed:(string)aClassName;

+(BOOL)instancesRespondTo:(SEL)aSel;
-(BOOL)respondsTo:(SEL)aSel;

+(BOOL)conformsTo:(Protocol)aProtocol;
-(BOOL)conformsTo:(Protocol)aProtocol;

+(IMP)instanceMethodFor:(SEL)aSel;
-(IMP)methodFor:(SEL)aSel;
//+(struct objc_method_description *)descriptionForInstanceMethod:(SEL)aSel;
//-(struct objc_method_description *)descriptionForMethod:(SEL)aSel;

-perform:(SEL)aSel;
-perform:(SEL)aSel with:anObject;
-perform:(SEL)aSel with:anObject1 with:anObject2;

//-(retval_t)forward:(SEL)aSel :(arglist_t)argFrame;
//-(retval_t)performv:(SEL)aSel :(arglist_t)argFrame;

+poseAs:(Class)aClassObject;
-(Class)transmuteClassTo:(Class)aClassObject;

-subclassResponsibility:(SEL)aSel;
-notImplemented:(SEL)aSel;
-shouldNotImplement:(SEL)aSel;

-doesNotRecognize:(SEL)aSel;
-error:(string)aString, ...;

//+(integer)version;
//+setVersion:(integer)aVersion;
//+(integer)streamVersion: (TypedStream*)aStream;

//-read: (TypedStream*)aStream;
//-write: (TypedStream*)aStream;
//-awake;
@end

@implementation Object
+initialize
{
	return self;
}

-init
{
	return self;
}

+new
{
	return [[self alloc] init];
}

+alloc
{
	return class_create_instance (self);
}

-free
{
	return object_dispose (self);
}

-copy
{
	return [[self shallowCopy] deepen];
}

-shallowCopy
{
	return object_copy (self);
}

-deepen
{
	return self;
}

-deepCopy
{
	return [self copy];
}

-(Class)class
{
	return object_get_class (self);
}

-(Class)superClass
{
	return object_get_super_class (self);
}

-(Class)metaClass
{
	return object_get_meta_class (self);
}

-(string)name
{
	return object_get_class_name (self);
}

-self
{
	return self;
}

-(integer)hash = #0;	// can't cast pointer to integer

-(BOOL)isEqual:anObject
{
	return id(self) == anObject;	//FIXME shouldn't need cast
}

-(integer)compare:anotherObject = #0;	// can only == or != pointers

-(BOOL)isMetaClass
{
	return NO;
}

-(BOOL)isClass
{
	return object_is_class (self);
}

-(BOOL)isInstance
{
	return object_is_instance (self);
}

-(BOOL)isKindOf:(Class)aClassObject
{
	local Class class;

	for (class = self.isa; class; class = class_get_super_class (class))
		if (class == aClassObject)
			return YES;
	return NO;
}

-(BOOL)isMemberOf:(Class)aClassObject
{
	return self.isa == aClassObject;
}

-(BOOL)isKindOfClassNamed:(string)aClassName
{
	local Class class;
	if (aClassName)
		for (class = self.isa; class; class = class_get_super_class (class))
			if (class_get_class_name (class) == aClassName)
				return YES;
	return NO;
}

-(BOOL)isMemberOfClassNamed:(string)aClassName
{
	local Class class;
	if (aClassName)
		for (class = self.isa; class; class = class_get_super_class (class))
			if (class_get_class_name (class) == aClassName)
				return YES;
	return aClassName && class_get_class_name (self.isa) == aClassName;
}

+(BOOL)instancesRespondTo:(SEL)aSel
{
	return class_get_instance_method (self, aSel) != NIL;
}

-(BOOL)respondsTo:(SEL)aSel
{
	return (object_is_instance (self)
			? class_get_instance_method (self.isa, aSel)
			: class_get_class_method (self.isa, aSel)) != NIL;
}

+(BOOL)conformsTo:(Protocol)aProtocol = #0;
-(BOOL)conformsTo:(Protocol)aProtocol
{
	return [[self class] conformsTo:aProtocol];
}

+(IMP)instanceMethodFor:(SEL)aSel
{
	return method_get_imp (class_get_instance_method (self, aSel));
}

-(IMP)methodFor:(SEL)aSel
{
	return method_get_imp (object_is_instance (self)
						   ? class_get_instance_method (self.isa, aSel)
						   : class_get_class_method (self.isa, aSel));
}

//+(struct objc_method_description *)descriptionForInstanceMethod:(SEL)aSel = #0;
//-(struct objc_method_description *)descriptionForMethod:(SEL)aSel = #0;

-perform:(SEL)aSel
{
	local IMP msg = obj_msg_lookup (self, aSel);

	if (!msg)
		return [self error:"invalid selector passed to %s",
				sel_get_name (_cmd)];
	return msg (self, aSel);
}

-perform:(SEL)aSel with:anObject
{
	local IMP msg = obj_msg_lookup (self, aSel);

	if (!msg)
		return [self error:"invalid selector passed to %s",
				sel_get_name (_cmd)];
	return msg (self, aSel, anObject);
}

-perform:(SEL)aSel with:anObject1 with:anObject2
{
	local IMP msg = obj_msg_lookup (self, aSel);

	if (!msg)
		return [self error:"invalid selector passed to %s",
				sel_get_name (_cmd)];
	return msg (self, aSel, anObject1, anObject2);
}

//-(retval_t)forward:(SEL)aSel :(arglist_t)argFrame = #0;
//-(retval_t)performv:(SEL)aSel :(arglist_t)argFrame = #0;

+poseAs:(Class)aClassObject
{
	return class_pose_as (self, aClassObject);
}

-(Class)transmuteClassTo:(Class)aClassObject
{
	if (object_is_instance (self))
		if (class_is_class (aClassObject))
			if (class_get_instance_size (aClassObject) == class_get_instance_size (isa))
				if ([self isKindOf:aClassObject]) {
					local Class old_isa = isa;
					isa = aClassObject;
					return old_isa;
				}
	return NIL;
}

-subclassResponsibility:(SEL)aSel
{
	return [self error:"subclass should override %s",
			sel_get_name(aSel)];
}

-notImplemented:(SEL)aSel
{
	return [self error:"methos %s not implemented",
			sel_get_name(aSel)];
}

-shouldNotImplement:(SEL)aSel
{
	return [self error:"%s should not implement %s",
				object_get_class_name (self), sel_get_name(aSel)];
}

-doesNotRecognize:(SEL)aSel
{
	return [self error:"%s does not recognize %s",
				object_get_class_name (self), sel_get_name(aSel)];
}

-error:(string)aString, ... = #0;

//+(integer)version = #0;
//+setVersion:(integer)aVersion = #0;
//+(integer)streamVersion: (TypedStream*)aStream = #0;

//-read: (TypedStream*)aStream = #0;
//-write: (TypedStream*)aStream = #0;
//-awake = #0;
@end
