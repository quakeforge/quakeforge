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
