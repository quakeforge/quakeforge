#include <Set.h>
#include "basisblade.h"
#include "basisgroup.h"

@implementation BasisGroup
+(BasisGroup *) new:(int) count basis:(BasisBlade **)blades
{
	BasisGroup *group = [[[BasisGroup alloc] init] autorelease];
	group.blades = obj_malloc (count * sizeof (BasisBlade *));
	group.set = [[Set set] retain];
	group.range = { ~0u, 0 };
	for (int i = 0; i < count; i++) {
		group.blades[i] = [blades[i] retain];
		unsigned    m = [blades[i] mask];
		if (m < group.range[0]) {
			group.range[0] = m;
		}
		if (m > group.range[1]) {
			group.range[1] = m;
		}
		[group.set add:m];
	}
	int         num = group.range[1] - group.range[0] + 1;
	group.map = obj_malloc (num * sizeof (int));
	for (int i = 0; i < count; i++) {
		group.map[[blades[i] mask] - group.range[0]] = i;
	}
	group.count = count;
	return group;
}

-(void)dealloc
{
	[set release];
	for (int i = 0; i < count; i++) {
		[blades[i] release];
	}
	obj_free (blades);
	[super dealloc];
}

-(int)count
{
	return count;
}

-(uivec2)blade_range
{
	return range;
}

-(BasisBlade *) bladeAt:(int) ind
{
	return blades[ind];
}

-(BasisBlade *) blade:(unsigned) mask
{
	if (![set is_member:mask]) {
		return nil;
	}
	return blades[map[mask - range[0]]];
}

-(Set *) set
{
	return set;
}
@end
