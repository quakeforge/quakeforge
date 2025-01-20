#include <PropertyList.h>
#include <string.h>

#include "vkfieldreadonly.h"
#include "vkfieldtype.h"
#include "vkgen.h"
#include "vktype.h"

@implementation ReadOnlyField

-init:(PLItem *) item struct:(Struct *)strct field:(string)fname
{
	self = [super init:item struct:strct field:fname];
	if (!self) {
		return self;
	}

	PLItem     *desc = [item getObjectForKey:"type"];
	type = [[FieldType fieldType:[desc getObjectAtIndex:1]] retain];

	value_field = str_hold ([[item getObjectForKey:"value"] string]);
	return self;
}

-(void) dealloc
{
	[type release];
	str_free (value_field);
	[super dealloc];
}

-writeParseData
{
	return self;
}

-writeField
{
	string      parse_type = [FieldType anyType];
	fprintf (output_file, "\t{\"%s\", 0, %s, parse_%s, 0},\n",
			 field_name, parse_type, "ignore");
	return self;
}

-writeSymbol
{
	fprintf (output_file,
			 "\t{\"%s\", %s, (void *) offsetof (%s, %s)},\n",
			 field_name, [type exprType], struct_name, value_field);
	return self;
}

@end
