#include <PropertyList.h>

#include "vkfieldlabeledarray.h"
#include "vkfieldtype.h"
#include "vkgen.h"
#include "vkstruct.h"
#include "vktype.h"

@implementation LabeledArrayField

-init:(PLItem *) item struct:(Struct *)strct field:(string)fname
{
	self = [super init:item struct:strct field:fname];
	if (!self) {
		return self;
	}

	PLItem     *desc = [item getObjectForKey:"type"];
	string      label_field = [[desc getObjectAtIndex:2] string];
	Type       *t = [[Type lookup:[type type]] resolveType];
	if ([t isKindOfClass:[Struct class]]) {
		Struct     *s = (Struct *) t;
		[s setLabelField:label_field];
	}

	return self;
}

-writeParse
{
	fprintf (output_file, "\t\tplfield_t   %s_field = {\n", field_name);
	fprintf (output_file, "\t\t\t.name = \"%s\",\n", field_name);
	fprintf (output_file, "\t\t\t.type = QFDictionary,\n");
	fprintf (output_file, "\t\t\t.data = &parse_%s_%s_data,\n",
			 struct_name, field_name);
	fprintf (output_file, "\t\t};\n");
	fprintf (output_file, "\t\tif (!parse_labeledarray (&%s_field, item, "
			 "data, messages, context)) {\n", field_name);
	fprintf (output_file, "\t\t\treturn 0;\n");
	fprintf (output_file, "\t\t}\n");
	return self;
}

-writeField
{
	fprintf (output_file, "\t{\"%s\", 0, %s, parse_%s, &parse_%s_%s_data},\n",
			 field_name, "QFDictionary", "labeledarray", struct_name,
			 field_name);
	return self;
}

@end
