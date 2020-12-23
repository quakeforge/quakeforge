#include <PropertyList.h>

#include "vkfieldsingle.h"
#include "vkgen.h"
#include "vktype.h"

@implementation SingleField

-init:(PLItem *) item struct:(Struct *)strct field:(string)fname
{
	self = [super init:item struct:strct field:fname];
	if (!self) {
		return self;
	}

	PLItem     *desc = [item getObjectForKey:"type"];
	type = [[desc getObjectAtIndex:1] string];

	value_field = [[item getObjectForKey:"value"] string];
	return self;
}

-writeParseData
{
	Type       *field_type = [[Type lookup: type] dereference];

	fprintf (output_file, "static parse_single_t parse_%s_%s_data = {\n",
			 struct_name, field_name);
	fprintf (output_file, "\t%s,\n", [field_type parseType]);
	fprintf (output_file, "\tsizeof (%s),\n", type);
	fprintf (output_file, "\tparse_%s,\n", type);
	fprintf (output_file, "\tfield_offset (%s, %s),\n",
			 struct_name, value_field);
	fprintf (output_file, "};\n");
	return self;
}

-writeField
{
	string      parse_type = [[[Type lookup: type] dereference] parseType];
	fprintf (output_file, "\t{\"%s\", 0, %s, parse_%s, &parse_%s_%s_data},\n",
			 field_name, parse_type, "single", struct_name, field_name);
	return self;
}

@end
