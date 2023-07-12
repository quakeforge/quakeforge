#include <PropertyList.h>

#include "vkfielddata.h"
#include "vkgen.h"
#include "vkstruct.h"
#include "vktype.h"

@implementation DataField

-init:(PLItem *) item struct:(Struct *)strct field:(string)fname
{
	self = [super init:item struct:strct field:fname];
	if (!self) {
		return self;
	}

	value_field = [[item getObjectForKey:"data"] string];
	size_field = [[item getObjectForKey:"size"] string];
	return self;
}

-writeParseData
{
	fprintf (output_file, "static parse_data_t parse_%s_%s_data = {\n",
			 struct_name, field_name);
	fprintf (output_file, "\tfield_offset (%s, %s),\n",
			 struct_name, value_field);
	if (size_field) {
		fprintf (output_file, "\tfield_offset (%s, %s),\n",
				 struct_name, size_field);
	} else {
		fprintf (output_file, "\tt-1,\n");
	}
	fprintf (output_file, "};\n");
	return self;
}

-writeField
{
	fprintf (output_file, "\t{\"%s\", 0, %s, parse_%s, &parse_%s_%s_data},\n",
			 field_name, "QFBinary", "data", struct_name, field_name);
	return self;
}

@end
