/**
	\defgroup runtime Objective Runtime
	\{
*/
#ifndef __ruamoko_runtime_h_
#define __ruamoko_runtime_h_

#define INT_MAX		2147483647
#define INT_MIN		(-INT_MAX - 1)

#define UINT_MAX 	4294967295U

enum {
	NotFound = INT_MAX	///< A value indicating a requested item can't be found.
};

/**
	Standard boolean type

	This is not like a boolean in some other languages, where you can compare
	any number against the 'true' value; that is, the following does not catch
	all true values.
	\code
	if (foo == YES) {
		...
	}
	\endcode
	If you want to catch all true values, simply test for truth using something
	like:
	\code
	if (foo) {
		...
	}
	\endcode
*/
typedef enum {
	NO = 0,		///< the false value
	YES			///< a true value
} BOOL;

@extern void __obj_forward(id obj, SEL sel, ...);
@extern BOOL __obj_responds_to(id obj, SEL sel);
@extern void obj_error (id object, int code, string fmt, ...);
@extern void obj_verror (id object, int code, string fmt, @va_list args);
//obj_error_handler obj_set_error_handler (objc_error_handler func);
@extern IMP obj_msg_lookup (id receiver, SEL op);
@extern IMP obj_msg_lookup_super (Super class, SEL op);
@extern @param obj_msg_sendv (id receiver, SEL op, @va_list args);
@extern BOOL obj_decrement_ref_was_zero (id object);
@extern BOOL obj_increment_ref_was_zero (id object);
@extern void *obj_malloc (int size);
@extern void *obj_atomic_malloc (int size);
@extern void *obj_valloc (int size);
@extern void *obj_realloc (void *mem, int size);
@extern void *obj_calloc (int nelem, int size);
@extern void obj_free (void *mem);
//(void *) obj_get_uninstalled_dtable (void);

@extern Class obj_get_class (string name);
@extern Class obj_lookup_class (string name);
//Class obj_next_class (void **enum_stage);

@extern string sel_get_name (SEL selector);
@extern string sel_get_type (SEL selector);
@extern SEL sel_get_uid (string name);
//@extern SEL sel_get_any_uid (string name);
//@extern SEL sel_get_any_typed_uid (string name);
//@extern SEL sel_get_typed_uid (string name, string type);
@extern SEL sel_register_name (string name);
//@extern SEL sel_register_typed_name (string name, string type);
@extern BOOL sel_is_mapped (SEL aSel);

@extern Method *class_get_class_method (Class class, SEL aSel);
@extern Method *class_get_instance_method (Class class, SEL aSel);
@extern Class class_pose_as (Class imposter, Class superclass);
@extern id class_create_instance (Class class);
@extern string class_get_class_name (Class class);
@extern int class_get_instance_size (Class class);
@extern Class class_get_meta_class (Class class);
@extern Class class_get_super_class (Class class);
@extern int class_get_version (Class class);
@extern BOOL class_is_class (Class class);
@extern BOOL class_is_meta_class (Class class);
@extern void class_set_version (Class class, int version);
@extern void *class_get_gc_object_type (Class class);
@extern void class_ivar_set_gcinvisible (Class class, string ivarname, BOOL gcInvisible);

@extern IMP method_get_imp (Method *method);
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

@extern void *PR_FindGlobal (string name);	//FIXME where?

// copies the list in src to a temporary buffer that will be freed when the
// calling function returns, and places the pointer to the buffer into the
// returned va_list. The count is copies as-is.
@extern @va_list va_copy(@va_list src);
#endif	//__ruamoko_runtime_h_
/**
	\}
*/
