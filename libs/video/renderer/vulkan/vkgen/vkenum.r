#include <string.h>

#include "vkenum.h"
#include "vkgen.h"

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
	if (!(self = [super initWithType: type])) {
		return nil;
	}
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
	if (!Hash_Find (processed_types, name)) {
		printf ("    +%s\n", name);
		Hash_Add (processed_types, (void *) name);
		[queue addObject: self];
	}
}

static int
skip_value(string name)
{
	return (str_str (name, "_MAX_ENUM") >= 0
			|| str_str (name, "_BEGIN_RANGE") >= 0
			|| str_str (name, "_END_RANGE") >= 0
			|| str_str (name, "_RANGE_SIZE") >= 0);
}

-(void) writeTable
{
	int         strip_bit = 0;
	if (str_mid([self name], -8) == "FlagBits") {
		strip_bit = 1;
	}

	fprintf (output_file, "static exprtype_t %s_type = {\n", [self name]);
	fprintf (output_file, "\t\"%s\",\n", [self name]);
	fprintf (output_file, "\tsizeof (int),\n");
	if (strip_bit) {
		fprintf (output_file, "\tflag_binops,\n");
		fprintf (output_file, "\tflag_unops,\n");
	} else {
		fprintf (output_file, "\t0,\n");
		fprintf (output_file, "\t0,\n");
	}
	fprintf (output_file, "};\n");

	fprintf (output_file, "static %s %s_values[] = {\n", [self name], [self name]);
	for (int i = 0, index = 0; i < type.strct.num_fields; i++) {
		qfot_var_t *var = &type.strct.fields[i];
		if (skip_value (var.name)) {
			continue;
		}
		fprintf (output_file, "\t%s,    // %d 0x%x\n",
				 var.name, var.offset, var.offset);
		index++;
	}
	fprintf (output_file, "};\n");
	fprintf (output_file, "static exprsym_t %s_symbols[] = {\n", [self name]);
	for (int i = 0, index = 0; i < type.strct.num_fields; i++) {
		qfot_var_t *var = &type.strct.fields[i];
		if (skip_value (var.name)) {
			continue;
		}
		fprintf (output_file, "\t{\"%s\", &%s_type, %s_values + %d},\n",
				 var.name, [self name], [self name], index);
		if (prefix_length) {
			string      shortname = str_mid (var.name, prefix_length);
			if (strip_bit) {
				int bit_pos = str_str (shortname, "_BIT");
				if (bit_pos >= 0) {
					shortname = str_mid (shortname, 0, bit_pos);
				}
			}
			fprintf (output_file, "\t{\"%s\", &%s_type, %s_values + %d},\n",
					 str_lower(shortname), [self name], [self name], index);
		}
		index++;
	}
	fprintf (output_file, "\t{ }\n");
	fprintf (output_file, "};\n");
	fprintf (output_file, "static exprtab_t %s_symtab = {\n", [self name]);
	fprintf (output_file, "\t%s_symbols,\n", [self name]);
	fprintf (output_file, "};\n");
	fprintf (output_file, "exprenum_t %s_enum = {\n", [self name]);
	fprintf (output_file, "\t&%s_type,\n", [self name]);
	fprintf (output_file, "\t&%s_symtab,\n", [self name]);
	fprintf (output_file, "};\n");

	fprintf (header_file, "extern exprenum_t %s_enum;\n", [self name]);
}

-(void) writeSymtabInit
{
	fprintf (output_file, "\tcexpr_init_symtab (&%s_symtab, context);\n",
			 [self name]);
}

-(string) cexprType
{
	return [self name] + "_type";
}

-(string) parseType
{
	return "QFString";
}

-(string) parseFunc
{
	return "parse_enum";
}

-(string) parseData
{
	return "&" + [self name] + "_enum";
}
@end
