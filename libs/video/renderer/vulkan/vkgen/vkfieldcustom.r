#include <PropertyList.h>
#include <string.h>

#include "vkfieldcustom.h"
#include "vkfieldtype.h"
#include "vkgen.h"
#include "vktype.h"

@implementation CustomField

-init:(PLItem *) item struct:(Struct *)strct field:(string)fname
{
	self = [super init:item struct:strct field:fname];
	if (!self) {
		return self;
	}

	PLItem     *desc = [item getObjectForKey:"type"];
	pltype = str_hold (parseItemType ([desc getObjectAtIndex:1]));
	parser = str_hold ([[desc getObjectAtIndex:2] string]);

	fields = [item getObjectForKey:"fields"];
	return self;
}

-(void)dealloc
{
	str_free (pltype);
	str_free (parser);
	[super dealloc];
}

-writeParseData
{
	fprintf (output_file, "static size_t parse_%s_%s_offsets[] = {\n",
			 struct_name, field_name);
	for (int i = 0, count = [fields count]; i < count; i++) {
		string field = [[fields getObjectAtIndex:i] string];
		fprintf (output_file, "\tfield_offset (%s, %s),\n",
				 struct_name, field);
	}
	fprintf (output_file, "};\n");

	fprintf (output_file, "static parse_custom_t parse_%s_%s_data = {\n",
			 struct_name, field_name);
	fprintf (output_file, "\t%s,\n", parser);
	fprintf (output_file, "\tparse_%s_%s_offsets,\n",
			 struct_name, field_name);
	fprintf (output_file, "\t%d,\n", [fields count]);
	fprintf (output_file, "};\n");
	return self;
}

-writeField
{
	fprintf (output_file, "\t{\"%s\", 0, %s, parse_%s, &parse_%s_%s_data},\n",
			 field_name, pltype, "custom", struct_name, field_name);
	return self;
}

-writeSymbol
{
	fprintf (output_file,
			 "\t{\"%s\", 0/*FIXME*/, 0/*(void *) field_offset (%s, %s)*/},\n",
			 field_name, struct_name, "FIXME");
	return self;
}

-(int) searchType
{
	return 0;
}

@end
