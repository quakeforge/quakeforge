#include <hash.h>
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

+ (PLItem *) fromItem:(plitem_t *) item
{
	return [[PLItem itemClass: item] autorelease];
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

@implementation PLArchive

typedef struct pla_ref_s {
	void *obj;
	string id;
} pla_ref_t;

static ulong
pla_get_hash_ptr (void *ele, void *data)
{
	pla_ref_t *ref = ele;
	return (ulong) (uint) ref.obj;
}

static bool
pla_compare_ptr (void *ele1, void *ele2, void *data)
{
	pla_ref_t *ref1 = ele1;
	pla_ref_t *ref2 = ele2;
	return ref1.obj == ref2.obj;
}

static string
pla_get_key (void *ele, void *data)
{
	pla_ref_t *ref = ele;
	return ref.id;
}

-init
{
	if (!(self = [super init])) {
		return self;
	}
	obj_refs = Hash_NewTable (1021, pla_get_key, nil, nil);
	Hash_SetHashCompare (obj_refs, pla_get_hash_ptr, pla_compare_ptr);
	return self;
}

+(PLArchive *)archive
{
	return [[[self alloc] init] autorelease];
}

-(string)id:(void *)ptr
{
	pla_ref_t ref = { .obj = ptr };
	pla_ref_t *r = Hash_FindElement (obj_refs, &ref);
	return r ? r.id : "";
}

-(string)add:(void *)ptr
{
	pla_ref_t ref = { .obj = ptr };
	pla_ref_t *r = Hash_FindElement (obj_refs, &ref);
	if (r) {
		return r.id;
	}
	r = obj_malloc (sizeof (pla_ref_t));
	*r = { .obj = ptr, .id = str_hold (sprintf ("%lx", ++id)) };
	Hash_Add (obj_refs, r);
	Hash_AddElement (obj_refs, r);
	return r.id;
}

-(void *)add:(void *)ptr withId:(string)id
{
	pla_ref_t *r = obj_malloc (sizeof (pla_ref_t));
	*r = { .obj = ptr, .id = str_hold (id) };
	Hash_Add (obj_refs, r);
	Hash_AddElement (obj_refs, r);
	return ptr;
}

-(void *)ptr:(string)id
{
	pla_ref_t *r = Hash_Find (obj_refs, id);
	return r ? r.obj : nil;
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
d_string_func (void *ptr, plitem_t *item, PLArchive *ctx)
{
	auto p = (string *) ptr;
	*p++ = str_hold (PL_String (item));
	return p;
}

static void *
d_float_func (void *ptr, plitem_t *item, PLArchive *ctx)
{
	auto p = (float *) ptr;
	*p++ = (float) PL_Number (item);
	return p;
}

static void *
d_func_func (void *ptr, plitem_t *item, PLArchive *ctx)
{
	auto p = (int *) ptr;
	string name = PL_String (item);
	*p++ = PR_FindFunction (name);
	return p;
}

static void *
d_ptr_func (void *ptr, plitem_t *item, qfot_type_t *type, PLArchive *ctx)
{
	type = unalias_type (type.fldptr.aux_type);
	id obj = nil;
	if (type.encoding == ":") {
		string name = PL_String (item);
		auto p = (SEL *) ptr;
		*p++ = sel_get_uid (name);
		return p;
	}
	if (type.meta == ty_class) {
		auto class = obj_lookup_class (type.class);
		obj = [[class fromPropertyList:item withArchive:ctx] retain];
	}
	auto p = (id *) ptr;
	*p++ = obj;
	return p;
}

static void *
d_double_func (void *ptr, plitem_t *item, PLArchive *ctx)
{
	auto p = (double *) ptr;
	*p++ = (double) PL_Number (item);
	return p;
}

static void *
d_int_func (void *ptr, plitem_t *item, PLArchive *ctx)
{
	auto p = (int *) ptr;
	*p++ = (int) PL_Number (item);
	return p;
}

static void *
d_uint_func (void *ptr, plitem_t *item, PLArchive *ctx)
{
	auto p = (uint *) ptr;
	*p++ = (uint) PL_Number (item);
	return p;
}

static void *
d_long_func (void *ptr, plitem_t *item, PLArchive *ctx)
{
	auto p = (long *) ptr;
	*p++ = (long) PL_Number (item);
	return p;
}

static void *
d_ulong_func (void *ptr, plitem_t *item, PLArchive *ctx)
{
	auto p = (ulong *) ptr;
	*p++ = (ulong) PL_Number (item);
	return p;
}

static void *
d_bool_func (void *ptr, plitem_t *item, PLArchive *ctx)
{
	auto p = (bool *) ptr;
	*p++ = PL_Bool (item);
	return p;
}

static void *
d_lbool_func (void *ptr, plitem_t *item, PLArchive *ctx)
{
	auto p = (bool *) ptr;
	*p++ = PL_Bool (item);
	return p;
}

static void
d_basic_func (qfot_type_t *type, void *ptr, plitem_t *item, PLArchive *ctx)
{
	if (str_char(type.encoding, 0) == '@') {
		*(id *)ptr = [[Object fromPropertyList:item withArchive:ctx] retain];
		return;
	}
	if (type.type == ev_ptr) {
		d_ptr_func (ptr, item, type, ctx);
	}
	static void *(*type_funcs[])(void *ptr, plitem_t *item, PLArchive *ctx) = {
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
	static int type_sizes[] = {
		[ev_string] = sizeof (string),
		[ev_float] = sizeof (float),
		[ev_vector] = sizeof (float),
		[ev_func] = sizeof (@function (void)),
		[ev_quaternion] = sizeof (float),
		[ev_int] = sizeof (int),
		[ev_uint] = sizeof (uint),
		[ev_double] = sizeof (double),
		[ev_long] = sizeof (long),
		[ev_ulong] = sizeof (ulong),
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
		int c_count = PL_A_NumObjects (item);
		for (int i = 0; i < columns && i < c_count; i++) {
			auto vec = PL_ObjectAtIndex (item, i);
			int count = PL_A_NumObjects (vec);
			for (int j = 0; j < width && j < count; j++) {
				ptr = func (ptr, PL_ObjectAtIndex (vec, j), ctx);
			}
			ptr = (void *)((int)ptr + type_sizes[type.type]);
		}
	} else if (width > 1) {
		int count = PL_A_NumObjects (item);
		for (int j = 0; j < width && j < count; j++) {
			ptr = func (ptr, PL_ObjectAtIndex (item, j), ctx);
		}
	} else {
		func (ptr, item, ctx);
	}
}

static void deserialize_value (qfot_type_t *type, void *ptr, plitem_t *item,
							   PLArchive *ctx);

static void
d_struct_func (qfot_type_t *type, void *ptr, plitem_t *item, PLArchive *ctx)
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
			deserialize_value (field.type, p, val, ctx);
		}
	}
}

static void
d_enum_func (qfot_type_t *type, void *ptr, plitem_t *item, PLArchive *ctx)
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
d_array_func (qfot_type_t *type, void *ptr, plitem_t *item, PLArchive *ctx)
{
}

static void
deserialize_value (qfot_type_t *type, void *ptr, plitem_t *item, PLArchive *ctx)
{
	static void (*meta_funcs[])(qfot_type_t *type, void *ptr, plitem_t *item,
								PLArchive *ctx)={
		[ty_basic] = d_basic_func,
		[ty_struct] = d_struct_func,
		[ty_enum] = d_enum_func,
		[ty_array] = d_array_func,
		[ty_bool] = d_basic_func,
	};
	type = unalias_type (type);
	meta_funcs[type.meta] (type, ptr, item, ctx);
}

@implementation Object (PLItem)
-(void)setDefaultIvars
{
}

+(id)fromPropertyList:(plitem_t *)plitem withArchive:(PLArchive *)ctx
{
	if (PL_Type (plitem) == QFDictionary) {
		if (auto ref = PL_ObjectForKey (plitem, "@ref")) {
			return (id) [ctx ptr:PL_String (ref)];
		}
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
		if (auto id = PL_ObjectForKey (plitem, "@id")) {
			[ctx add:obj withId:PL_String (id)];
		}
		[obj setDefaultIvars];

		Class class = [self class];
		while (class && class != [Object class]) {
			int *ivar_ptr = class.ivars;
			int ivar_count = *ivar_ptr;
			auto ivar = (struct obj_ivar *) (ivar_ptr + 1);
			for (int i = 0; i < ivar_count; i++, ivar++) {
				auto item = PL_ObjectForKey (plitem, ivar.ivar_name);
				if (item) {
					deserialize_value (ivar.ivar_type, obj + ivar.ivar_offset,
									   item, ctx);
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
			[arr addObject:[Object fromPropertyList:obj withArchive:ctx]];
		}
		return arr;
	}
	return nil;
}

static plitem_t *
s_string_func (@inout void *ptr, PLArchive *ctx)
{
	auto p = (string *) ptr;
	auto item = PL_NewString (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_float_func (@inout void *ptr, PLArchive *ctx)
{
	auto p = (float *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_func_func (@inout void *ptr, PLArchive *ctx)
{
	auto p = (int *) ptr;
	string name = PR_FunctionName (*p++);
	auto item = PL_NewString (name);
	ptr = p;
	return item;
}

static plitem_t *
s_double_func (@inout void *ptr, PLArchive *ctx)
{
	auto p = (double *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_int_func (@inout void *ptr, PLArchive *ctx)
{
	auto p = (int *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_uint_func (@inout void *ptr, PLArchive *ctx)
{
	auto p = (uint *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_long_func (@inout void *ptr, PLArchive *ctx)
{
	auto p = (long *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_ulong_func (@inout void *ptr, PLArchive *ctx)
{
	auto p = (ulong *) ptr;
	auto item = PL_NewNumber (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_ptr_func (void *ptr, qfot_type_t *type, PLArchive *ctx)
{
	type = unalias_type (type.fldptr.aux_type);
	if (type.encoding == ":") {
		return PL_NewString (sel_get_name (*(SEL*) ptr));
	}
	if (type.meta != ty_class) {
		return nil;
	}
	auto obj = [*(id*)ptr serialize:ctx];
	if (obj && !PL_ObjectForKey (obj, "@ref")
		&& type.class != class_get_class_name ([*(id*)ptr class])) {
		auto clsname = object_get_class_name (*(id*)ptr);
		auto str = PL_NewString (clsname);
		PL_D_AddObject (obj, "@classname", str);
	}
	return obj;
}

static plitem_t *
s_bool_func (@inout void *ptr, PLArchive *ctx)
{
	auto p = (bool *) ptr;
	auto item = PL_NewBool (*p++);
	ptr = p;
	return item;
}

static plitem_t *
s_lbool_func (@inout void *ptr, PLArchive *ctx)
{
	auto p = (lbool *) ptr;
	auto item = PL_NewBool (*p++);
	ptr = p;
	return item;
}


static plitem_t *
s_basic_func (qfot_type_t *type, void *ptr, PLArchive *ctx)
{
	if (str_char(type.encoding, 0) == '@') {
		// id
		auto obj = [*(id*)ptr serialize:ctx];
		if (obj && !PL_ObjectForKey (obj, "@ref")) {
			auto clsname = object_get_class_name (*(id*)ptr);
			PL_D_AddObject (obj, "@classname", PL_NewString (clsname));
		}
		return obj;
	}
	if (type.type == ev_ptr) {
		return s_ptr_func (ptr, type, ctx);
	}
	//FIXME qfcc doesn't typecheck the function initializers properly
	static plitem_t *(*type_funcs[])(@inout void *ptr, PLArchive *ctx) = {
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
				PL_A_AddObject (vec, func (ptr, ctx));
			}
			PL_A_AddObject (mat, vec);
		}
		return mat;
	} else if (width > 1) {
		auto vec = PL_NewArray ();
		for (int i = 0; i < width; i++) {
			PL_A_AddObject (vec, func (ptr, ctx));
		}
		return vec;
	} else {
		return func (ptr, ctx);
	}
}

static plitem_t *serialize_value (qfot_type_t *type, void *ptr, PLArchive *ctx);

static plitem_t *
s_struct_func (qfot_type_t *type, void *ptr, PLArchive *ctx)
{
	auto strct = PL_NewDictionary ();
	for (int i = 0; i < type.strct.num_fields; i++) {
		auto field = &type.strct.fields[i];
		if (str_char (field.name, 0) == '.') {
			// skip .anonymous
			// NOTE: unions are UB
			continue;
		}
		auto obj = serialize_value (field.type, (int *)ptr + field.offset, ctx);
		if (obj) {
			PL_D_AddObject (strct, field.name, obj);
		}
	}
	return strct;
}

static plitem_t *
s_enum_func (qfot_type_t *type, void *ptr, PLArchive *ctx)
{
	for (int i = 0; i < type.strct.num_fields; i++) {
		if (type.strct.fields[i].offset == *(int *) ptr) {
			return PL_NewString (type.strct.fields[i].name);
		}
	}
	return PL_NewNumber (*(int *) ptr);
}

static plitem_t *
s_array_func (qfot_type_t *type, void *ptr, PLArchive *ctx)
{
	return nil;
}

static plitem_t *
serialize_value (qfot_type_t *type, void *ptr, PLArchive *ctx)
{
	static plitem_t *(*meta_funcs[]) (qfot_type_t *type, void *ptr,
									  PLArchive *ctx) = {
		[ty_basic] = s_basic_func,
		[ty_struct] = s_struct_func,
		[ty_enum] = s_enum_func,
		[ty_array] = s_array_func,
		[ty_bool] = s_basic_func,
	};
	type = unalias_type (type);
	auto obj = meta_funcs[type.meta] (type, ptr, ctx);
	return obj;
}

-(plitem_t *)serialize:(PLArchive *)ctx
{
	if (string ref = [ctx id:self]) {
		plitem_t *pl = PL_NewDictionary ();
		PL_D_AddObject (pl, "@ref", PL_NewString (ref));
		return pl;
	}
	plitem_t *pl = PL_NewDictionary ();
	auto clsname = object_get_class_name (self);
	PL_D_AddObject (pl, "@classname", PL_NewString (clsname));
	PL_D_AddObject (pl, "@id", PL_NewString ([ctx add:self]));

	Class cls = [self class];
	while (cls && cls != [Object class]) {
		int *ivar_ptr = cls.ivars;
		int ivar_count = *ivar_ptr;
		auto ivar = (struct obj_ivar *) (ivar_ptr + 1);
		for (int i = 0; i < ivar_count; i++, ivar++) {
			auto o = serialize_value (ivar.ivar_type, self + ivar.ivar_offset,
									  ctx);
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
+(id)fromPropertyList:(plitem_t *)plitem withArchive:(PLArchive *)ctx
{
	uint count = PL_A_NumObjects (plitem);
	auto array = [Array arrayWithCapacity:count];
	for (uint i = 0; i < count; i++) {
		auto obj = PL_ObjectAtIndex (plitem, i);
		[array addObject: [Object fromPropertyList:obj withArchive:ctx]];
	}
	return array;
}

-(plitem_t *)serialize:(PLArchive *)ctx
{
	if (string ref = [ctx id:self]) {
		plitem_t *pl = PL_NewDictionary ();
		PL_D_AddObject (pl, "@ref", PL_NewString (ref));
		return pl;
	}
	auto array = PL_NewArray ();
	for (uint i = 0; i < count; i++) {
		id obj = _objs[i];
		auto item = [obj serialize:ctx];
		PL_A_AddObject (array, item);
	}
	return array;
}
@end
