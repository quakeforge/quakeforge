#include <PropertyList.h>
#include <string.h>

#include "vkfieldtype.h"
#include "vkgen.h"
#include "vktype.h"

@implementation FieldType

+fieldType:(PLItem *)item
{
	return [[[self alloc] initWithItem:item] autorelease];
}

-(void)dealloc
{
	str_free (type);
	str_free (parser);
	str_free (parse_type);
}

static string
parseItemType (PLItem *item)
{
	string      str = [item string];
	if (str) {
		return str;
	}
	string      mask = "QFMultiType";
	for (int i = [item count]; i-- > 0; ) {
		str = [[item getObjectAtIndex:i] string];
		mask = mask + " | (1 << " + str + ")";
	}
	return str_hold (mask);
}

-initWithItem:(PLItem *)item
{
	if (!(self = [super init])) {
		return nil;
	}

	type = [item string];
	if (type) {
		Type       *field_type = [[Type lookup:type] dereference];
		type = str_hold (type);
		parse_type = [field_type parseType];
		parser = str_hold ("parse_" + type);
	} else {
		parse_type = parseItemType([item getObjectForKey:"parse_type"]);
		type = str_hold ([[item getObjectForKey:"type"] string]);
		parser = str_hold ([[item getObjectForKey:"parser"] string]);
	}

	return self;
}

-writeParseData
{
	fprintf (output_file, "\t%s,\n", parse_type);
	fprintf (output_file, "\tsizeof (%s),\n", type);
	fprintf (output_file, "\t%s,\n", parser);
	return self;
}

-(string) parseType
{
	return parse_type;
}

@end
