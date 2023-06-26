#include <string.h>

#include "vkfieldauto.h"
#include "vkgen.h"
#include "vkstruct.h"

@implementation AutoField

-init:(PLItem *) item struct:(Struct *)strct field:(string)fname
{
	self = [super init:item struct:strct field:fname];
	if (!self) {
		return self;
	}
	string real_name = [[item getObjectForKey:"field"] string];
	field = [strct findField:real_name ? real_name : field_name];

	return self;
}

-writeParseData
{
	//printf("FieldDef: '%s' '%s'\n", struct_name, field_name);
	return self;
}

-writeField
{
	Type       *field_type = [Type findType: field.type];
	fprintf (output_file, "\t{\"%s\", field_offset (%s, %s), %s, %s, %s},\n",
			 field_name, struct_name, field.name,
			 [field_type parseType], [field_type parseFunc],
			 [field_type parseData]);
	return self;
}

-writeSymbol
{
	Type       *field_type = [Type findType: field.type];
	fprintf (output_file,
			 "\t{\"%s\", &%s, (void *) field_offset (%s, %s)},\n",
			 field_name, [field_type cexprType], struct_name, field.name);
	return self;
}

@end
