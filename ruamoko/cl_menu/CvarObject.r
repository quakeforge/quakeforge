#include "string.h"

#include "CvarObject.h"

@implementation CvarObject
-(id)init
{
	self = [super init];
	name = str_new ();
	return self;
}

-(id)initWithCvar:(string)cvname
{
	self = [self init];
	str_copy (name, cvname);
	return self;
}

-(void)dealloc
{
	str_free (name);
	[super dealloc];
}
@end
