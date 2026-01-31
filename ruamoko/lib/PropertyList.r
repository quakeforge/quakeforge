#include <PropertyList.h>
#include <types.h>
#include <string.h>
#include <Array.h>

@implementation PLItem

+ (PLItem *) newDictionary
{
	return [PLDictionary new];
}

+ (PLItem *) newArray
{
	return [PLArray new];
}

+ (PLItem *) newData:(void*) data size:(int) len
{
	return [PLData new:data size:len];
}

+ (PLItem *) newString:(string) str
{
	return [PLString new:str];
}

+ itemClass:(plitem_t *) item
{
	local string classname = nil;
	local id class;

	if (!item)
		return nil;
	switch (PL_Type (item)) {
	case QFDictionary:
		classname = "PLDictionary";
		break;
	case QFArray:
		classname = "PLArray";
		break;
	case QFBinary:
		classname = "PLData";
		break;
	case QFString:
		classname = "PLString";
		break;
	default:
		return nil;
	}
	class = obj_lookup_class (classname);
	return [[class alloc] initWithItem: item];
}

+ (PLItem *) fromString:(string) str
{
	return [[PLItem itemClass: PL_GetPropertyList (str)] autorelease];
}

+ (PLItem *) fromFile:(QFile) file
{
	return [[PLItem itemClass: PL_GetFromFile (file)] autorelease];
}

- initWithItem:(plitem_t *) item
{
	if (!(self = [super init]))
		return self;
	PL_Retain (item);
	self.item = item;
	return self;
}

- (void) dealloc
{
	PL_Release (item);
	[super dealloc];
}

- (plitem_t *) item
{
	return item;
}

- (string) write
{
	return PL_WritePropertyList (item);
}

- (pltype_t) type
{
	return PL_Type (item);
}

- (int) line
{
	return PL_Line (item);
}

- (int) count
{
	if ([self class] == [PLDictionary class]) {
		return PL_D_NumKeys (item);
	} else {
		return PL_A_NumObjects (item);
	}
}

- (int) numKeys
{
	return PL_D_NumKeys (item);
}

- (PLItem *) getObjectForKey:(string) key
{
	return [[PLItem itemClass: PL_ObjectForKey (item, key)] autorelease];
}

- (PLItem *) allKeys
{
	return [[PLItem itemClass: PL_D_AllKeys (item)] autorelease];
}

- (string) keyAtIndex:(int) index
{
	return PL_KeyAtIndex (item, index);
}

- addKey:(string) key value:(PLItem *) value
{
	PL_D_AddObject (item, key, value.item);
	return self;
}

- (int) numObjects
{
	return PL_A_NumObjects (item);
}

- (PLItem *) getObjectAtIndex:(int) index
{
	return [[PLItem itemClass: PL_ObjectAtIndex (item, index)] autorelease];
}

- addObject:(PLItem *) object
{
	PL_A_AddObject (item, object.item);
	return self;
}

- insertObject:(PLItem *) object atIndex:(int) index
{
	PL_A_InsertObjectAtIndex (item, object.item, index);
	return self;
}

- (string) string
{
	return PL_String (item);
}

- (double) number
{
	return PL_Number (item);
}

- (bool) bool
{
	return PL_Bool (item);
}

@end


@implementation PLDictionary

+ (PLDictionary *) new
{
	return [[PLDictionary alloc] initWithItem: PL_NewDictionary ()];
}

+ (PLItem *) fromString:(string) str
{
	return [[PLItem itemClass: PL_GetDictionary (str)] autorelease];
}

+ (PLItem *) fromFile:(QFile) file
{
	return [[PLItem itemClass: PL_GetDictionaryFromFile (file)] autorelease];
}

- (int) count
{
	return PL_D_NumKeys (item);
}

- (int) numKeys
{
	return PL_D_NumKeys (item);
}

- (PLItem *) getObjectForKey:(string) key
{
	return [[PLItem itemClass: PL_ObjectForKey (item, key)] autorelease];
}

- (PLItem *) allKeys
{
	return [[PLItem itemClass: PL_D_AllKeys (item)] autorelease];
}

- addKey:(string) key value:(PLItem *) value
{
	PL_D_AddObject (item, key, value.item);
	return self;
}

@end

@implementation PLArray

+ (PLArray *) new
{
	return [[PLArray alloc] initWithItem: PL_NewArray ()];
}

+ (PLItem *) fromString:(string) str
{
	return [[PLItem itemClass: PL_GetArray (str)] autorelease];
}

+ (PLItem *) fromFile:(QFile) file
{
	return [[PLItem itemClass: PL_GetArrayFromFile (file)] autorelease];
}

- (int) count
{
	return PL_A_NumObjects (item);
}

- (int) numObjects
{
	return PL_A_NumObjects (item);
}

- (PLItem *) getObjectAtIndex:(int) index
{
	return [[PLItem itemClass: PL_ObjectAtIndex (item, index)] autorelease];
}

- addObject:(PLItem *) object
{
	PL_A_AddObject (item, object.item);
	return self;
}

- insertObject:(PLItem *) object atIndex:(int) index
{
	PL_A_InsertObjectAtIndex (item, object.item, index);
	return self;
}

@end

@implementation PLData

+ (PLData *) new:(void*) data size:(int) len
{
	return [[PLData alloc] initWithItem: PL_NewData (data, len)];
}

@end

@implementation PLString

+ (PLString *) new: (string) str
{
	return [[PLString alloc] initWithItem: PL_NewString (str)];
}

- (string) string
{
	return PL_String (item);
}

@end

@implementation PLNumber
+ (PLNumber *) new:(double) val
{
	return [[PLNumber alloc] initWithItem: PL_NewNumber (val)];
}

- (double) number
{
	return PL_Number (item);
}
@end

@implementation PLBool
+ (PLBool *) new:(bool) val
{
	return [[PLNumber alloc] initWithItem: PL_NewBool (val)];
}

- (bool) bool
{
	return PL_Bool (item);
}
@end

@implementation PLNull
+ (PLNull *) new
{
	return [[PLNull alloc] initWithItem: PL_NewNull ()];
}
@end

string PR_FunctionName (uint func) = #0;
int PR_FindFunction (string name) = #0;

static qfot_type_t *
unalias_type (qfot_type_t *type)
{
	while (type.meta == ty_alias) {
		type = type.alias.aux_type;
	}
	return type;
}

static void *
d_string_func (void *ptr, plitem_t *item)
{
	auto p = (string *) ptr;
	*p++ = str_hold (PL_String (item));
	return p;
}

static void *
d_float_func (void *ptr, plitem_t *item)
{
	auto p = (float *) ptr;
	*p++ = (float) PL_Number (item);
	return p;
}

static void *
d_func_func (void *ptr, plitem_t *item)
{
	auto p = (int *) ptr;
	string name = PL_String (item);
	*p++ = PR_FindFunction (name);
	return p;
}

static void *
d_ptr_func (void *ptr, plitem_t *item, qfot_type_t *type)
{
	type = unalias_type (type.fldptr.aux_type);
	id obj = nil;
	if (type.meta == ty_class) {
		auto class = obj_lookup_class (type.class);
		obj = [class fromPropertyList:item];
	}
	auto p = (id *) ptr;
	*p++ = obj;
	return p;
}

static void *
d_double_func (void *ptr, plitem_t *item)
{
	auto p = (double *) ptr;
	*p++ = (double) PL_Number (item);
	return p;
}

static void *
d_int_func (void *ptr, plitem_t *item)
{
	auto p = (int *) ptr;
	*p++ = (int) PL_Number (item);
	return p;
}

static void *
d_uint_func (void *ptr, plitem_t *item)
{
	auto p = (uint *) ptr;
	*p++ = (uint) PL_Number (item);
	return p;
}

static void *
d_long_func (void *ptr, plitem_t *item)
{
	auto p = (long *) ptr;
	*p++ = (long) PL_Number (item);
	return p;
}

static void *
d_ulong_func (void *ptr, plitem_t *item)
{
	auto p = (ulong *) ptr;
	*p++ = (ulong) PL_Number (item);
	return p;
}

static void *
d_bool_func (void *ptr, plitem_t *item)
{
	auto p = (bool *) ptr;
	*p++ = PL_Bool (item);
	return p;
}

static void *
d_lbool_func (void *ptr, plitem_t *item)
{
	auto p = (bool *) ptr;
	*p++ = PL_Bool (item);
	return p;
}

static void
d_basic_func (qfot_type_t *type, void *ptr, plitem_t *item)
{
	if (str_char(type.encoding, 0) == '@') {
		*(id *)ptr = [Object fromPropertyList:item];
		return;
	}
	if (type.type == ev_ptr) {
		d_ptr_func (ptr, item, type);
	}
	static void *(*type_funcs[])(void *ptr, plitem_t *item) = {
		[ev_string] = d_string_func,
		[ev_float] = d_float_func,
		[ev_vector] = d_float_func,
		[ev_func] = d_func_func,
		[ev_quaternion] = d_float_func,
		[ev_int] = d_int_func,
		[ev_uint] = d_uint_func,
		[ev_double] = d_double_func,
		[ev_long] = d_long_func,
		[ev_ulong] = d_ulong_func,
	};
	int columns = type.basic.columns;
	int width = type.basic.width;
	if (type.type == ev_vector) {
		width = 3;
	}
	if (type.type == ev_quaternion) {
		width = 4;
	}
	if (type.type == ev_func) {
		width = columns = 1;
	}
	auto func = type_funcs[type.type];
	if (type.meta == ty_bool) {
		func = type.type == ev_int ? d_bool_func : d_lbool_func;
	}
	if (!func) {
		return;
	}
	if (columns > 1) {
		for (int i = 0; i < width; i++) {
			auto vec = PL_ObjectAtIndex (item, i);
			for (int j = 0; j < width; j++) {
				ptr = func (ptr, PL_ObjectAtIndex (vec, j));
			}
		}
	} else if (width > 1) {
		for (int j = 0; j < width; j++) {
			ptr = func (ptr, PL_ObjectAtIndex (item, j));
		}
	} else {
		func (ptr, item);
	}
}

static void deserialize_value (qfot_type_t *type, void *ptr, plitem_t *item);

static void
d_struct_func (qfot_type_t *type, void *ptr, plitem_t *item)
{
	for (int i = 0; i < type.strct.num_fields; i++) {
		auto field = &type.strct.fields[i];
		if (str_char (field.name, 0) == '.') {
			// skip .anonymous
			// NOTE: unions are UB
			continue;
		}
		auto val = PL_ObjectForKey (item, field.name);
		if (val) {
			auto p = (int *)ptr + field.offset;
			deserialize_value (field.type, p, val);
		}
	}
}

static void
d_enum_func (qfot_type_t *type, void *ptr, plitem_t *item)
{
	int val = 0;
	if (PL_Type (item) == QFString) {
		string name = PL_String (item);
		for (int i = 0; i < type.strct.num_fields; i++) {
			if (type.strct.fields[i].name == name) {
				val = type.strct.fields[i].offset;
				break;
			}
		}
	} else {
		val = (int) PL_Number (item);
	}
	*(int *) ptr = val;
}

static void
d_array_func (qfot_type_t *type, void *ptr, plitem_t *item)
{
}

static void
deserialize_value (qfot_type_t *type, void *ptr, plitem_t *item)
{
	static void (*meta_funcs[])(qfot_type_t *type, void *ptr, plitem_t *item)={
		[ty_basic] = d_basic_func,
		[ty_struct] = d_struct_func,
		[ty_enum] = d_enum_func,
		[ty_array] = d_array_func,
		[ty_bool] = d_basic_func,
	};
	type = unalias_type (type);
	meta_funcs[type.meta] (type, ptr, item);
}

@implementation Object (PLItem)
+(id)fromPropertyList:(plitem_t *)plitem
{
	if (PL_Type (plitem) == QFDictionary) {
		auto classname = PL_ObjectForKey (plitem, "@classname");
		if (classname) {
			string c = PL_String (classname);
			if (c && c != object_get_class_name (self)) {
				if (!(self = obj_lookup_class (c))) {
					return nil;
				}
			}
		}
		// emulate Object init
		id obj = [[[self alloc] retain] autorelease];

		Class class = [self class];
		while (class && class != [Object class]) {
			int *ivar_ptr = class.ivars;
			int ivar_count = *ivar_ptr;
			auto ivar = (struct obj_ivar *) (ivar_ptr + 1);
			for (int i = 0; i < ivar_count; i++, ivar++) {
				//FIXME ((qfot_type_t*)ivar.ivar_type).encoding crashes qfcc
				// (not that it's needed here anyway)
				auto item = PL_ObjectForKey (plitem, ivar.ivar_name);
				if (item) {
					deserialize_value (ivar.ivar_type, obj + ivar.ivar_offset,
									   item);
				}
			}
			class = class.super_class;
		}
		return obj;
	} else if (PL_Type (plitem) == QFArray) {
		id arr = [Array array];
		int count = PL_A_NumObjects (plitem);
		for (int i = 0; i < count; i++) {
			auto obj = PL_ObjectAtIndex (plitem, i);
			[arr addObject:[Object fromPropertyList:obj]];
		}
		return arr;
	}
	return nil;
}

static plitem_t *
s_string_func (@inout void *ptr)
{
	auto p = (string *) ptr;
	auto item = PL_NewString (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_float_func (@inout void *ptr)
{
	auto p = (float *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_func_func (@inout void *ptr)
{
	auto p = (int *) ptr;
	string name = PR_FunctionName (*p++);
	auto item = PL_NewString (name);
	ptr = p;
	return item;
}

static plitem_t *
s_double_func (@inout void *ptr)
{
	auto p = (double *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_int_func (@inout void *ptr)
{
	auto p = (int *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_uint_func (@inout void *ptr)
{
	auto p = (uint *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_long_func (@inout void *ptr)
{
	auto p = (long *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_ulong_func (@inout void *ptr)
{
	auto p = (ulong *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_ptr_func (void *ptr, qfot_type_t *type)
{
	type = unalias_type (type.fldptr.aux_type);
	if (type.meta != ty_class) {
		return nil;
	}
	auto obj = [*(id*)ptr serialize];
	if (obj && type.class != class_get_class_name ([*(id*)ptr class])) {
		auto clsname = object_get_class_name (*(id*)ptr);
		auto str = PL_NewString (clsname);
		PL_D_AddObject (obj, "@classname", str);
	}
	return obj;
}

static plitem_t *
s_bool_func (@inout void *ptr)
{
	auto p = (bool *) ptr;
	auto item = PL_NewBool (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_lbool_func (@inout void *ptr)
{
	auto p = (lbool *) ptr;
	auto item = PL_NewBool (*p++);
	ptr = p;
	return item;
}


static plitem_t *
s_basic_func (qfot_type_t *type, void *ptr)
{
	if (str_char(type.encoding, 0) == '@') {
		// id
		auto obj = [*(id*)ptr serialize];
		if (obj) {
			auto clsname = object_get_class_name (*(id*)ptr);
			PL_D_AddObject (obj, "@classname", PL_NewString (clsname));
		}
		return obj;
	}
	if (type.type == ev_ptr) {
		return s_ptr_func (ptr, type);
	}
	//FIXME qfcc doesn't typecheck the function initializers properly
	static plitem_t *(*type_funcs[])(@inout void *ptr) = {
		[ev_string] = s_string_func,
		[ev_float] = s_float_func,
		[ev_vector] = s_float_func,
		[ev_func] = s_func_func,
		[ev_quaternion] = s_float_func,
		[ev_int] = s_int_func,
		[ev_uint] = s_uint_func,
		[ev_double] = s_double_func,
		[ev_long] = s_long_func,
		[ev_ulong] = s_ulong_func,
	};
	int columns = type.basic.columns;
	int width = type.basic.width;
	if (type.type == ev_vector) {
		width = 3;
	}
	if (type.type == ev_quaternion) {
		width = 4;
	}
	if (type.type == ev_func) {
		width = columns = 1;
	}
	auto func = type_funcs[type.type];
	if (type.meta == ty_bool) {
		func = type.type == ev_int ? s_bool_func : s_lbool_func;
	}
	if (!func) {
		return nil;
	}
	if (columns > 1) {
		auto mat = PL_NewArray ();
		for (int i = 0; i < columns; i++) {
			auto vec = PL_NewArray ();
			for (int i = 0; i < width; i++) {
				PL_A_AddObject (vec, func (ptr));
			}
			PL_A_AddObject (mat, vec);
		}
		return mat;
	} else if (width > 1) {
		auto vec = PL_NewArray ();
		for (int i = 0; i < width; i++) {
			PL_A_AddObject (vec, func (ptr));
		}
		return vec;
	} else {
		return func (ptr);
	}
}

static plitem_t *serialize_value (qfot_type_t *type, void *ptr);

static plitem_t *
s_struct_func (qfot_type_t *type, void *ptr)
{
	auto strct = PL_NewDictionary ();
	for (int i = 0; i < type.strct.num_fields; i++) {
		auto field = &type.strct.fields[i];
		if (str_char (field.name, 0) == '.') {
			// skip .anonymous
			// NOTE: unions are UB
			continue;
		}
		auto obj = serialize_value (field.type, (int *)ptr + field.offset);
		if (obj) {
			PL_D_AddObject (strct, field.name, obj);
		}
	}
	return strct;
}

static plitem_t *
s_enum_func (qfot_type_t *type, void *ptr)
{
	for (int i = 0; i < type.strct.num_fields; i++) {
		if (type.strct.fields[i].offset == *(int *) ptr) {
			return PL_NewString (type.strct.fields[i].name);
		}
	}
	return PL_NewNumber (*(int *) ptr);
}

static plitem_t *
s_array_func (qfot_type_t *type, void *ptr)
{
	return nil;
}

static plitem_t *
serialize_value (qfot_type_t *type, void *ptr)
{
	static plitem_t *(*meta_funcs[]) (qfot_type_t *type, void *ptr) = {
		[ty_basic] = s_basic_func,
		[ty_struct] = s_struct_func,
		[ty_enum] = s_enum_func,
		[ty_array] = s_array_func,
		[ty_bool] = s_basic_func,
	};
	type = unalias_type (type);
	auto obj = meta_funcs[type.meta] (type, ptr);
	return obj;
}

-(plitem_t *)serialize
{
	plitem_t *pl = PL_NewDictionary ();
	auto clsname = object_get_class_name (self);
	PL_D_AddObject (pl, "@classname", PL_NewString (clsname));

	Class cls = [self class];
	while (cls && cls != [Object class]) {
		int *ivar_ptr = cls.ivars;
		int ivar_count = *ivar_ptr;
		auto ivar = (struct obj_ivar *) (ivar_ptr + 1);
		for (int i = 0; i < ivar_count; i++, ivar++) {
			//FIXME ((qfot_type_t*)ivar.ivar_type).encoding crashes qfcc
			// (not that it's needed here anyway)
			auto o = serialize_value (ivar.ivar_type, self + ivar.ivar_offset);
			if (o) {
				PL_D_AddObject (pl, ivar.ivar_name, o);
			}
		}
		cls = cls.super_class;
	}
	return pl;
}
@end

@implementation Array (PLItem)
+(id)fromPropertyList:(plitem_t *)plitem
{
	uint count = PL_A_NumObjects (plitem);
	auto array = [Array arrayWithCapacity:count];
	for (uint i = 0; i < count; i++) {
		auto obj = PL_ObjectAtIndex (plitem, i);
		[array addObject: [Object fromPropertyList:obj]];
	}
	return array;
}

-(plitem_t *)serialize
{
	auto array = PL_NewArray ();
	for (uint i = 0; i < count; i++) {
		id obj = _objs[i];
		auto item = [obj serialize];
		PL_A_AddObject (array, item);
	}
	return array;
}
@end
