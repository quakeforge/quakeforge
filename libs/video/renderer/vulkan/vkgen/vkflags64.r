#include <string.h>

#include "vkflags64.h"
#include "vkgen.h"

typedef struct xdef_s {
	qfot_type_t *type;          ///< pointer to type definition
	void       *ofs;            ///< 32-bit version of ddef_t.ofs
} xdef_t;

typedef struct xdefs_s {
	xdef_t     *xdefs;
	unsigned    num_xdefs;
} xdefs_t;

@implementation FlagBits64 : Object
-initWithName:(string)name value:(ulong)value
{
	if (!(self = [super init])) {
		return nil;
	}
	self.name = str_hold (name);
	self.value = value;
	return self;
}

-(void)dealloc
{
	str_free (name);
	[super dealloc];
}

+(FlagBits64 *)flagbits:(string)name value:(ulong)value
{
	return [[[FlagBits64 alloc] initWithName:name value:value] autorelease];
}

-(string)name
{
	return name;
}

-(ulong)value
{
	return value;
}
@end

@implementation Flags64
-(void)process
{
	if (members) {
		// already found the members
		return;
	}
	members = [[Array array] retain];
	xdefs_t *xdefs = PR_FindGlobal (".xdefs");
	for (unsigned i = 0; i < xdefs.num_xdefs; i++) {
		auto xdef = &xdefs.xdefs[i];
		if (xdef.type == type) {
			ddef_t ddef = PR_GlobalAtOfs (xdef.ofs);
			auto m = [FlagBits64 flagbits:ddef.name value:*(ulong *)xdef.ofs];
			[members addObject:m];
		}
	}
}

+(Type *)fromName:(string)name
{
	int ind = str_str (name, "Flags");
	if (ind > 0) {
		string end = str_mid (name, ind + 5);
		string tag = str_mid (name, 0, ind + 4) + "Bits" + end;
		return [(id) Hash_Find (available_types, tag) resolveType];
	}
	return nil;
}

-initWithType: (qfot_type_t *) type
{
	if (!(self = [super initWithType: type])) {
		return nil;
	}
	string name = [self name];
	int start = 2;	// Vk
	int fb = str_str (name, "FlagBits");
	int ver = fb + 8;
	int ver_end = ver;
	int c = str_char (name, ver_end);
	while (c >= '0' && c <= '9') {
		c = str_char (name, ++ver_end);
	}
	int caps = 0;
	for (int i = start; i < fb; i++) {
		c = str_char (name, i);
		if (c >= 'A' && c <= 'Z') {
			caps++;
		}
	}
	prefix_length = start + caps + fb - start + ver_end - ver;
	if (ver_end - ver) {
		prefix_length += 2; // _ on either side
	}
	return self;
}

-(void)dealloc
{
	[members release];
	[super dealloc];
}

-(string) name
{
	return type.alias.name;
}

-(void) addToQueue
{
	string name = [self name];
	if (!Hash_Find (processed_types, name)) {
		[self process];
		Hash_Add (processed_types, (void *) name);
		[queue addObject: self];
	}
}

-(int) isEmpty
{
	return !members || ![members count];
}

-(void) writeTable
{
	fprintf (output_file, "exprtype_t %s_type = {\n", [self name]);
	fprintf (output_file, "\t.name = \"%s\",\n", [self name]);
	fprintf (output_file, "\t.size = sizeof (VkFlags64),\n");
	fprintf (output_file, "\t.binops = cexpr_flag64_binops,\n");
	fprintf (output_file, "\t.unops = cexpr_flag64_unops,\n");
	fprintf (output_file, "\t.get_string = cexpr_flags64_get_string,\n");
	fprintf (output_file, "\t.data = &%s_enum,\n", [self name]);
	fprintf (output_file, "};\n");

	uint num_members = [members count];
	if (![self isEmpty]) {
		fprintf (output_file, "static %s %s_values[] = {\n", [self name], [self name]);
		for (uint i = 0; i < num_members; i++) {
			FlagBits64 *m = [members objectAtIndex:i];
			string name = [m name];
			ulong value = [m value];
			fprintf (output_file, "\t%s,    // %ld 0x%lx\n",
					 name, value, value);
		}
		fprintf (output_file, "};\n");
	}
	fprintf (output_file, "static exprsym_t %s_symbols[] = {\n", [self name]);
	for (uint i = 0; i < num_members; i++) {
		FlagBits64 *m = [members objectAtIndex:i];
		string name = [m name];
		fprintf (output_file, "\t{\"%s\", &%s_type, %s_values + %d},\n",
				 name, [self name], [self name], i);

		if (prefix_length) {
			string shortname = str_mid (name, prefix_length);
			int bit_pos = str_str (shortname, "_BIT");
			if (bit_pos >= 0) {
				shortname = str_mid (shortname, 0, bit_pos);
			}
			fprintf (output_file, "\t{\"%s\", &%s_type, %s_values + %d},\n",
					 str_lower(shortname), [self name], [self name], i);
		}
	}
	fprintf (output_file, "\t{ }\n");
	fprintf (output_file, "};\n");
	fprintf (output_file, "static exprtab_t %s_symtab = {\n", [self name]);
	fprintf (output_file, "\t%s_symbols,\n", [self name]);
	fprintf (output_file, "};\n");
	fprintf (output_file, "static exprenum_t %s_enum = {\n", [self name]);
	fprintf (output_file, "\t.type = &%s_type,\n", [self name]);
	fprintf (output_file, "\t.symtab = &%s_symtab,\n", [self name]);
	fprintf (output_file, "};\n");

	fprintf (output_file, "static plfield_t %s_field = { 0, 0, QFString,"
			 " parse_enum, &%s_enum};\n",
			 [self name], [self name]);
	fprintf (output_file, "int parse_%s (const plfield_t *field,"
			 " const plitem_t *item, void *data, plitem_t *messages,"
			 " void *context)\n",
			 [self name]);
	fprintf (output_file, "{\n");
	fprintf (output_file,
			 "\treturn parse_enum (&%s_field, item, data, messages,"
			 " context);\n",
			 [self name]);
	fprintf (output_file, "}\n");

	fprintf (header_file, "int parse_%s (const plfield_t *field,"
			 " const plitem_t *item, void *data, plitem_t *messages,"
			 " void *context);\n",
			 [self name]);
	fprintf (header_file, "extern exprtype_t %s_type;\n", [self name]);
}
@end
