#include <hash.h>
#include <qfile.h>
#include <runtime.h>
#include <string.h>
#include <types.h>
#include <Array.h>
#include <AutoreleasePool.h>
#include <PropertyList.h>

#include "vkgen.h"
#include "vkstruct.h"
#include "vkenum.h"

static AutoreleasePool *autorelease_pool;
static void
arp_start (void)
{
	autorelease_pool = [[AutoreleasePool alloc] init];
}

static void
arp_end (void)
{
	[autorelease_pool release];
	autorelease_pool = nil;
}

void printf (string fmt, ...) = #0;

void fprintf (QFile file, string format, ...)
{
	Qputs (file, vsprintf (format, va_copy (@args)));
}

hashtab_t *available_types;
hashtab_t *processed_types;
Array *queue;
Array *output_types;

PLItem *parse;
QFile output_file;
QFile header_file;

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
	Type       *type = [Type findType:var.type];
	[type addToQueue];
}

void
scan_types (void)
{
	qfot_type_t *type;

	for (type = encodings.types;
		 ((int *)type - (int *) encodings.types) < encodings.size;
		 type = next_type (type)) {
		if (type.size) {
			string tag = str_mid(type.strct.tag, 4);
			Type *avail_type = [Type fromType: type];
			if (avail_type) {
				Hash_Add (available_types, avail_type);
			}
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

void
usage (string name)
{
	printf ("%s [plist file] [output file] [header file]\n", name);
}

int
main(int argc, string *argv)
{
	int         do_struct = 0;
	int         do_enum = 0;
	string      plist_filename;
	QFile       plist_file;
	PLItem     *plist;
	PLItem     *search;

	arp_start ();

	if (argc != 4) {
		usage (argv[0]);
		return 1;
	}
	plist_filename = argv[1];
	plist_file = Qopen (plist_filename, "rt");
	if (!plist_file) {
		printf ("could not open property list file: %s\n", plist_filename);
		return 1;
	}
	plist = [[PLItem fromFile: plist_file] retain];
	if (!plist) {
		printf ("error parsing: %s\n", plist_filename);
		return 1;
	}
	Qclose (plist_file);
	if ([plist class] != [PLDictionary class]) {
		printf ("%s not a dictionary\n", plist_filename);
	}
	search = [[plist getObjectForKey: "search"] retain];
	parse = [[plist getObjectForKey: "parse"] retain];

	encodings = PR_FindGlobal (".type_encodings");
	if (!encodings) {
		printf ("Can't find encodings\n");
		return 1;
	}
	queue = [[Array array] retain];
	output_types = [[Array array] retain];
	available_types = Hash_NewTable (127, get_object_key, nil, nil);
	processed_types = Hash_NewTable (127, get_string_key, nil, nil);
	scan_types ();

	for (int i = [search numObjects]; i-- > 0; ) {
		PLString   *str = (PLString *) [search getObjectAtIndex:i];
		string search_name = [str string];
		id obj = (id) Hash_Find (available_types, search_name);
		obj = [obj resolveType];
		printf("obj: %d %s\n", obj, class_get_class_name([obj class]));
		if (obj && [obj class] == [Struct class]) {
			[obj addToQueue];
		}
	}

	while ([queue count]) {
		id obj = [queue objectAtIndex:0];
		[queue removeObjectAtIndex:0];
		if ([obj class] == [Struct class]) {
			if ([[parse getObjectForKey:[obj name]] string] == "skip") {
				continue;
			}
			[obj forEachFieldCall:struct_func];
		}
		[output_types addObject:obj];
	}

	for (int i = 0; i < argc; i++) {
		printf ("vkgen %d %s\n", i, argv[i]);
	}

	arp_end ();

	output_file = Qopen (argv[2], "wt");
	header_file = Qopen (argv[3], "wt");
	for (int i = [output_types count]; i-- > 0; ) {
		id obj = [output_types objectAtIndex:i];
		if ([obj name] == "VkStructureType") {
			continue;
		}
		if ([obj class] != [Enum class]) {
			continue;
		}

		arp_start ();
		[obj writeTable];
		arp_end ();
	}
	for (int i = [output_types count]; i-- > 0; ) {
		id obj = [output_types objectAtIndex:i];
		if ([obj name] == "VkStructureType") {
			continue;
		}
		if ([obj class] != [Struct class]) {
			continue;
		}

		arp_start ();
		[obj writeTable];
		arp_end ();
	}
	fprintf (output_file, "static void\n");
	fprintf (output_file, "vkgen_init_symtabs (exprctx_t *context)\n");
	fprintf (output_file, "{\n");
	for (int i = [output_types count]; i-- > 0; ) {
		id obj = [output_types objectAtIndex:i];
		if ([obj name] == "VkStructureType") {
			continue;
		}
		arp_start ();
		[obj writeSymtabInit];
		[obj writeSymtabEntry];
		arp_end ();
	}
	fprintf (output_file, "}\n");
	Qclose (output_file);
	Qclose (header_file);
	return 0;
}
