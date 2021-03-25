#include <PropertyList.h>

#include "vkfieldstring.h"
#include "vkgen.h"

@implementation StringField

-init:(PLItem *) item struct:(Struct *)strct field:(string)fname
{
	self = [super init:item struct:strct field:fname];
	if (!self) {
		return self;
	}

	value_field = [[item getObjectForKey:"string"] string];
	return self;
}

-writeParseData
{
	fprintf (output_file, "static parse_string_t parse_%s_%s_data = {\n",
			 struct_name, field_name);
	fprintf (output_file, "\tfield_offset (%s, %s),\n",
			 struct_name, value_field);
	fprintf (output_file, "};\n");
	return self;
}

-writeField
{
	fprintf (output_file, "\t{\"%s\", 0, %s, parse_%s, &parse_%s_%s_data},\n",
			 field_name, "QFString", "string", struct_name, field_name);
	return self;
}

@end
