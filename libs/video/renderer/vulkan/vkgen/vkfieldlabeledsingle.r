#include <PropertyList.h>

#include "vkfieldlabeledsingle.h"
#include "vkfieldtype.h"
#include "vkgen.h"
#include "vkstruct.h"
#include "vktype.h"

@implementation LabeledSingleField

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

-writeField
{
	fprintf (output_file, "\t{\"%s\", 0, %s, parse_%s, &parse_%s_%s_data},\n",
			 field_name, "QFDictionary", "labeledsingle", struct_name,
			 field_name);
	return self;
}

@end
