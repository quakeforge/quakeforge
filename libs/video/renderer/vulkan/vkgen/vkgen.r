#include <hash.h>
#include <qfile.h>
#include <runtime.h>
#include <string.h>
#include <types.h>
#include <Array.h>

void printf (string fmt, ...) = #0;

void fprintf (QFile file, string format, ...)
{
	printf("fprintf %d\n", @args.count);
	Qputs (file, vsprintf (format, va_copy (@args)));
}

typedef void varfunc (qfot_var_t *var);

string search_names[] = {
	"VkGraphicsPipelineCreateInfo",
	"VkComputePipelineCreateInfo",
};
#define numsearch (sizeof (search_names) / sizeof (search_names[0]))
hashtab_t *available_types;
hashtab_t *processed_types;
Array *queue;
Array *output_types;

QFile output_file;

@interface Enum: Object
{
	qfot_type_t *type;
	int          prefix_length;
}
-initWithType: (qfot_type_t *) type;
/**	\warning	returned string is ephemeral
*/
-(string) name;
@end

@implementation Enum
-(void)process
{
	string      end = "_MAX_ENUM";
	int         len;
	string      prefix = nil;

	if (str_mid([self name], -8) == "FlagBits") {
		end = "_FLAG_BITS_MAX_ENUM";
	}
	len = -strlen (end);
	for (int i = 0; i < type.strct.num_fields; i++) {
		qfot_var_t *var = &type.strct.fields[i];
		if (str_mid (var.name, len) == end) {
			// len is negative so +1 consumes 1 more char (_)
			prefix = str_hold (str_mid (var.name, 0, len + 1));
		}
	}
	if (prefix) {
		prefix_length = strlen (prefix);
	}
}

-initWithType: (qfot_type_t *) type
{
	if (!(self = [super init])) {
		return nil;
	}
	self.type = type;
	[self process];
	return self;
}

-(string) name
{
	return str_mid(type.strct.tag, 4);
}

-(void) addToQueue
{
	string name = [self name];
	//printf ("    +%s\n", name);
	if (!Hash_Find (processed_types, name)) {
		Hash_Add (processed_types, (void *) name);
		[queue addObject: self];
	}
}

-(void) writeTable
{
	int         strip_bit = 0;
	if (str_mid([self name], -8) == "FlagBits") {
		strip_bit = 1;
	}

	fprintf (output_file, "enumval_t %s_values[] = {\n", [self name]);
	for (int i = 0; i < type.strct.num_fields; i++) {
		qfot_var_t *var = &type.strct.fields[i];
		if (str_str (var.name, "_MAX_ENUM") >= 0
			|| str_str (var.name, "_BEGIN_RANGE") >= 0
			|| str_str (var.name, "_END_RANGE") >= 0
			|| str_str (var.name, "_RANGE_SIZE") >= 0) {
			continue;
		}
		fprintf (output_file, "\t{\"%s\", %d},\n", var.name, var.offset);
		if (prefix_length) {
			string      shortname = str_mid (var.name, prefix_length);
			if (strip_bit) {
				int bit_pos = str_str (shortname, "_BIT");
				if (bit_pos >= 0) {
					shortname = str_mid (shortname, 0, bit_pos);
				}
			}
			fprintf (output_file, "\t{\"%s\", %d},\n", str_lower(shortname),
					 var.offset);
		}
	}
	fprintf (output_file, "\t{ }\n");
	fprintf (output_file, "};\n");
}
@end

@interface Struct: Object
{
	qfot_type_t *type;
}
-initWithType: (qfot_type_t *) type;
/**	\warning	returned string is ephemeral
*/
-(string) name;
-(void) forEachFieldCall: (varfunc) func;
@end

@implementation Struct
-initWithType: (qfot_type_t *) type
{
	if (!(self = [super init])) {
		return nil;
	}
	self.type = type;
	return self;
}

-(string) name
{
	return str_mid(type.strct.tag, 4);
}

-(void) forEachFieldCall: (varfunc) func
{
	qfot_struct_t *strct =&type.strct;

	for (int i = 0; i < strct.num_fields; i++) {
		func (&strct.fields[i]);
	}
}

-(void) addToQueue
{
	string name = [self name];
	//printf ("    +%s\n", name);
	if (!Hash_Find (processed_types, name)) {
		Hash_Add (processed_types, (void *) name);
		[queue addObject: self];
	}
}

-(void) writeTable
{
	fprintf (output_file, "structfld_t %s_fields[] = {\n", [self name]);
	for (int i = 0; i < type.strct.num_fields; i++) {
		qfot_var_t *var = &type.strct.fields[i];
		if (var.name == "sType" || var.name == "pNext") {
			continue;
		}
		string type_name = var.type.encoding;
		fprintf (output_file,
				 "\t{\"%s\", field_offset (%s, %s), %s, %s}, // %s\n",
				 var.name, [self name], var.name, "0", "0", type_name);
	}
	fprintf (output_file, "\t{ }\n");
	fprintf (output_file, "};\n");
}
@end

qfot_type_encodings_t *encodings;

qfot_type_t *
next_type (qfot_type_t *type)
{
	int         size = type.size;
	if (!size)
		size = 4;
	return (qfot_type_t *) ((int *) type + size);
}

int
type_is_null (qfot_type_t *type)
{
	return type.size == 0;
}

void print_type (qfot_type_t *type)
{
	printf ("type: %p %d %d %s", type, type.meta, type.size, type.encoding);
	switch (type.meta) {
		case ty_basic:
			printf (" %d", type.type);
			switch (type.type) {
				case ev_pointer:
				case ev_field:
					printf (" ");
					print_type (type.fldptr.aux_type);
					break;
				case ev_func:
					printf (" %p %d\n", type.func.return_type,
							type.func.num_params);
				default:
					printf ("\n");
					break;
			}
			break;
		case ty_struct:
		case ty_union:
		case ty_enum:
			printf (" %s %d\n", type.strct.tag, type.strct.num_fields);
			break;
		case ty_array:
			printf (" %p %d %d\n", type.array.type, type.array.base,
					type.array.size);
			break;
		case ty_class:
			printf (" %s\n", type.class);
			break;
		case ty_alias:
			printf (" %d %s ", type.alias.type, type.alias.name);
			print_type (type.alias.aux_type);
			break;
	}
}

void struct_func (qfot_var_t *var)
{
	qfot_type_t *alias;
	if (var.type.meta == ty_enum) {
		printf("%s a\n", var.name);
	} else if (var.type.meta == ty_struct) {
		printf("%s b\n", var.name);
	} else if (var.type.meta == ty_union) {
		printf("%s c\n", var.name);
	} else if (var.type.meta == ty_alias) {
		//printf("%s d\n", var.name);
		// typedef fun ;)
		alias = var.type.alias.full_type;
		if (alias.meta == ty_alias) {
			// double(+) typedef
			if (alias.alias.name == "VkFlags") {
				// enum flag fun :P
				string tag = var.type.alias.name;
				if (str_mid (tag, -5) == "Flags") {
					tag = str_mid (tag, 0, -1) + "Bits";
					id enumObj = (id) Hash_Find (available_types, tag);
					if (enumObj) {
						[enumObj addToQueue];
					}
				}
			} else {
				if (var.type.alias.name == "VkBool32") {
					// drop
				} else {
					printf ("%s =%s\n", var.name, var.type.alias.name);
				}
			}
		} else if (alias.meta == ty_enum) {
			string tag = str_mid(alias.strct.tag, 4);
			id enumObj = (id) Hash_Find (available_types, tag);
			if (enumObj) {
				[enumObj addToQueue];
			}
		} else if (alias.meta == ty_basic && alias.type == ev_pointer) {
			//printf("e\n");
			qfot_type_t *type = alias.fldptr.aux_type;
			if (type.meta == ty_alias) {
				id structObj = (id) Hash_Find (available_types, type.alias.name);
				//printf ("    a:%s %p\n", type.alias.name, structObj);
				if (structObj) {
					[structObj addToQueue];
				}
			} else if (type_is_null (type)) {
				// pointer to opaque struct. Probably VK_DEFINE_NON_DISPATCHABLE_HANDLE or VK_DEFINE_HANDLE
				//drop
				string createInfo = var.type.alias.name + "CreateInfo";
				id structObj = (id) Hash_Find (available_types, createInfo);
				if (structObj) {
					[structObj addToQueue];
				} else {
					//printf("    b:%s\n", var.type.alias.name);
				}
			} else {
				print_type (type);
				printf("%s c:%s:%s\n", var.name, var.type.alias.name, pr_type_name[alias.type]);
			}
		} else if (alias.meta == ty_basic) {
			//drop
			//printf("    d:%s:%s\n", var.type.alias.name, pr_type_name[alias.type]);
		} else if (alias.meta == ty_struct) {
			string tag = str_mid(alias.strct.tag, 4);
			id structObj = (id) Hash_Find (available_types, tag);
			if (structObj) {
				[structObj addToQueue];
			}
		} else {
			printf("    d:%s:%s\n", var.type.alias.name, ty_meta_name[alias.meta]);
		}
	} else if (var.type.meta == ty_basic && var.type.type == ev_pointer) {
		qfot_type_t *type = var.type.fldptr.aux_type;
		if (type.meta == ty_struct || type.meta == ty_union) {
			printf("%s f\n", var.name);
		} else if (type.meta == ty_alias) {
			printf("%s --- %s\n", var.name, type.alias.name);
			id obj = (id) Hash_Find (available_types, type.alias.name);
			if (obj && [obj class] == [Struct class]) {
				[obj forEachFieldCall:struct_func];
			}
		} else if (type.meta == ty_basic && type.type == ev_void) {
			//drop
		} else {
			printf("%s g\n", var.name);
			print_type (var.type);
		}
	} else {
		//drop
		//printf("    %s:%s:%s\n", var.name, ty_meta_name[var.type.meta], pr_type_name[var.type.type]);
	}
}

void
scan_types (void)
{
	qfot_type_t *type;

	for (type = encodings.types;
		 ((int *)type - (int *) encodings.types) < encodings.size;
		 type = next_type (type)) {
		if (!type.size
			|| (type.meta != ty_enum
				&& type.meta != ty_struct
				&& type.meta != ty_union)) {
			continue;
		}
		string tag = str_mid(type.strct.tag, 4);
		switch (type.meta) {
			case ty_enum:
				Hash_Add (available_types, [[Enum alloc] initWithType: type]);
				//printf ("+ enum %s\n", tag);
				break;
			case ty_struct:
			case ty_union:
				Hash_Add (available_types, [[Struct alloc] initWithType: type]);
				//printf ("+ struct %s\n", tag);
				break;
			case ty_basic:
			case ty_array:
			case ty_class:
			case ty_alias:
				break;
		}
	 }
}

static string
get_string_key (void *str, void *unused)
{
	return (string) str;
}

static string
get_object_key (void *obj, void *unused)
{
	return [(id)obj name];
}

int
main(int argc, string *argv)
{
	encodings = PR_FindGlobal (".type_encodings");
	if (!encodings) {
		printf ("Can't find encodings\n");
		return 1;
	}
	queue = [[Array alloc] init];
	output_types = [[Array alloc] init];
	available_types = Hash_NewTable (127, get_object_key, nil, nil);
	processed_types = Hash_NewTable (127, get_string_key, nil, nil);
	scan_types ();

	for (int i = 0;
		 i < sizeof (search_names) / sizeof (search_names[0]); i++) {
		id obj = (id) Hash_Find (available_types, search_names[i]);
		if (obj && [obj class] == [Struct class]) {
			[obj addToQueue];
		}
	}

	while ([queue count]) {
		id obj = [queue objectAtIndex:0];
		[queue removeObjectAtIndex:0];
		if ([obj class] == [Struct class]) {
			[obj forEachFieldCall:struct_func];
		}
		[output_types addObject:obj];
	}

	for (int i = 0; i < argc; i++) {
		printf ("vkgen %d %s\n", i, argv[i]);
	}

	output_file = Qopen (argv[1], "wt");
	for (int i = [output_types count]; i-- > 0; ) {
		id obj = [output_types objectAtIndex:i];
		if ([obj name] == "VkStructureType") {
			continue;
		}
		if ([obj class] == [Struct class]) {
			continue;
		}
		[obj writeTable];
	}
	Qclose (output_file);
	return 0;
}
