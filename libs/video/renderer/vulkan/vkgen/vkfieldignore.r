#include <string.h>
#include <PropertyList.h>

#include "vkfieldignore.h"
#include "vkfieldtype.h"
#include "vkstruct.h"

@implementation IgnoreField

-init:(PLItem *)item struct:(Struct *)strct field:(string)fname
{
	self = [super init];
	if (!self) {
		return self;
	}

	line = [item line];
	struct_name = str_hold ([strct outname]);
	field_name = str_hold (fname);
	return self;
}

-fromField:(qfot_var_t *)field struct:(Struct *)strct
{
	return self;
}

-(void)dealloc
{
	str_free (struct_name);
	str_free (field_name);
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
	return self;
}

-(string) name
{
	return field_name;
}

-(int) searchType
{
	return 1;
}

@end
