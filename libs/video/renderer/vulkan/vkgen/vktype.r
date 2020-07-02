#include <hash.h>

#include "vkgen.h"
#include "vktype.h"

@implementation Type

-initWithType: (qfot_type_t *) type
{
	if (!(self = [super init])) {
		return nil;
	}
	self.type = type;
	return self;
}

-(string) name
{
	//FIXME extract alias name and return proper type name
	return type.encoding;
}

-(void) addToQueue
{
	string name = [self name];
	//printf ("    +%s\n", name);
	if (!Hash_Find (processed_types, name)) {
		Hash_Add (processed_types, (void *) name);
		[queue addObject: self];
	}
}

@end
