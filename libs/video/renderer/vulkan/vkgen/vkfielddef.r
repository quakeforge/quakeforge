#include <string.h>
#include <PropertyList.h>

#include "vkfieldarray.h"
#include "vkfieldauto.h"
#include "vkfieldcustom.h"
#include "vkfielddata.h"
#include "vkfielddef.h"
#include "vkfieldreadonly.h"
#include "vkfieldsingle.h"
#include "vkfieldstring.h"
#include "vkstruct.h"

@implementation FieldDef

+fielddef:(PLItem *)item struct:(Struct *)strct field:(string)fname
{
	string      record = [item string];
	PLItem     *type_desc = [item getObjectForKey:"type"];

	if (!item) {
		record = "auto";
	}
	if (!record) {
		if (item && !type_desc) {
			return nil;
		}
		record = [type_desc string];
		if (!record) {
			record = [[type_desc getObjectAtIndex:0] string];
		}
	}
	switch (record) {
		case "auto":
			return [[[AutoField alloc] init:item struct:strct field:fname] autorelease];
		case "custom":
			return [[[CustomField alloc] init:item struct:strct field:fname] autorelease];
		case "string":
			return [[[StringField alloc] init:item struct:strct field:fname] autorelease];
		case "data":
			return [[[DataField alloc] init:item struct:strct field:fname] autorelease];
		case "single":
			return [[[SingleField alloc] init:item struct:strct field:fname] autorelease];
		case "array":
			return [[[ArrayField alloc] init:item struct:strct field:fname] autorelease];
		case "readonly":
			return [[[ReadOnlyField alloc] init:item struct:strct field:fname] autorelease];
	}
	return nil;
}

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
	fprintf (output_file, "undefined record type parse: %d\n", line);
	return self;
}

-writeField
{
	fprintf (output_file, "undefined record type field: %d\n", line);
	return self;
}

-writeSymbol
{
	fprintf (output_file,
			 "\t{\"%s\", 0/*FIXME*/, (void *) field_offset (%s, %s)},\n",
			 field_name, struct_name, value_field);
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
