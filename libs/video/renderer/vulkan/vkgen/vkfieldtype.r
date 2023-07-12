#include <PropertyList.h>
#include <string.h>

#include "vkfieldtype.h"
#include "vkgen.h"
#include "vktype.h"

string
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
	return mask;
}

@implementation FieldType

+fieldType:(PLItem *)item
{
	return [[[self alloc] initWithItem:item] autorelease];
}

-(void)dealloc
{
	str_free (type);
	str_free (parser);
	str_free (data);
	str_free (parse_type);
	[super dealloc];
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
		PLItem *typeItem = [item getObjectForKey:"parse_type"];
		parse_type = str_hold (parseItemType(typeItem));
		type = str_hold ([[item getObjectForKey:"type"] string]);
		parser = str_hold ([[item getObjectForKey:"parser"] string]);
		data = str_hold ([[item getObjectForKey:"data"] string]);
	}

	return self;
}

-writeParseData
{
	fprintf (output_file, "\t%s,\n", parse_type);
	fprintf (output_file, "\tsizeof (%s),\n", type);
	fprintf (output_file, "\t%s,\n", parser);
	if (data) {
		fprintf (output_file, "\t&%s,\n", data);
	} else {
		fprintf (output_file, "\t0,\n");
	}
	return self;
}

-(string) type
{
	return type;
}

-(string) exprType
{
	return "&" + type + "_type";
}

-(string) parseType
{
	return parse_type;
}

+(string) anyType
{
	string      mask = "QFMultiType"
						" | (1 << QFString)"
						" | (1 << QFBinary)"
						" | (1 << QFArray)"
						" | (1 << QFDictionary)";
	return mask;
}

@end
