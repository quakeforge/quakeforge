#ifndef __ruamoko_Object_h
#define __ruamoko_Object_h

typedef enum {
	NO,
	YES,
} BOOL;

@extern void (id object, integer code, string fmt, ...) obj_error;
@extern void (id object, integer code, string fmt, @va_list args) obj_verror;
//obj_error_handler (objc_error_handler func) obj_set_error_handler = #0;
@extern IMP (id receiver, SEL op) obj_msg_lookup;
@extern IMP (Super class, SEL op) obj_msg_lookup_super;
//retval_t (id receiver, SEL op, @va_list args) obj_msg_sendv;
@extern (void []) (integer size) obj_malloc;
@extern (void []) (integer size) obj_atomic_malloc;
@extern (void []) (integer size) obj_valloc;
@extern (void []) (void [] mem, integer size) obj_realloc;
@extern (void []) (integer nelem, integer size) obj_calloc;
@extern void (void [] mem) obj_free;
//(void []) (void) obj_get_uninstalled_dtable = #0;

@extern Class (string name) obj_get_class;
@extern Class (string name) obj_lookup_class;
//Class (void [][] enum_stage) obj_next_class = #0;

@extern string (SEL selector) sel_get_name;
@extern string (SEL selector) sel_get_type;
@extern SEL (string name) sel_get_uid;
@extern SEL (string name) sel_get_any_uid;
@extern SEL (string name) sel_get_any_typed_uid;
@extern SEL (string name, string type) sel_get_typed_uid;
@extern SEL (string name) sel_register_name;
@extern SEL (string name, string type) sel_register_typed_name;
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

#endif //__ruamoko_Object_h
