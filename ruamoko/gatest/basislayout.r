#include <stdlib.h>
#include <Set.h>
#include "basisblade.h"
#include "basisgroup.h"
#include "basislayout.h"
#include "util.h"

@implementation BasisLayout
+(BasisLayout *) new:(int) count groups:(BasisGroup **)groups
{
	BasisLayout *layout = [[[BasisLayout alloc] init] autorelease];
	layout.count = count;
	layout.groups = obj_malloc (count * sizeof (BasisGroup *));
	layout.set = [[Set set] retain];
	layout.range = { ~0u, 0 };
	int        *group_base = obj_malloc ((count + 1) * sizeof (int));
	group_base[0] = 0;
	int         num_blades = 0;
	for (int i = 0; i < count; i++) {
		layout.groups[i] = [groups[i] retain];
		[layout.set union:[groups[i] set]];
		group_base[i + 1] = [groups[i] count];
		num_blades += group_base[i + 1];

		uivec2      r = [groups[i] blade_range];
		if (r[0] < layout.range[0]) {
			layout.range[0] = r[0];
		}
		if (r[1] > layout.range[1]) {
			layout.range[1] = r[1];
		}
	}
	prefixsum (group_base, count);
	layout.blade_count = num_blades;
	layout.group_map = obj_malloc (num_blades * sizeof (ivec3));

	int         num = layout.range[1] - layout.range[0] + 1;
	layout.mask_map = obj_malloc (num * sizeof (int));
	int *group_inds = obj_malloc ((count + 1) * sizeof (int));
	for (int i = 0; i < count; i++) {
		BasisGroup *g = groups[i];
		group_inds[i] = 0;
		for (int j = 0; j < [g count]; j++) {
			BasisBlade *b = [g bladeAt:j];
			layout.mask_map[[b mask] - layout.range[0]] = group_inds[count];
			layout.group_map[group_inds[count]][0] = i;
			layout.group_map[group_inds[count]][1] = group_inds[i]++;
			layout.group_map[group_inds[count]][2] = group_base[i];
			group_inds[count]++;
		}
	}
	obj_free (group_inds);
	return layout;
}

-(void)dealloc
{
	[set release];
	for (int i = 0; i < count; i++) {
		[groups[i] release];
	}
	obj_free (groups);
	obj_free (group_map);
	[super dealloc];
}

-(int)count
{
	return count;
}

-(int)num_components
{
	int         num_components = 0;
	// assumes there is no overlap
	for (int i = 0; i < count; i++) {
		num_components += [groups[i] count];
	}
	return num_components;
}

-(int)blade_count
{
	return blade_count;
}

-(BasisGroup *) group:(int) ind
{
	return groups[ind];
}

-(BasisBlade *) bladeAt:(int) ind
{
	ivec3       gm = group_map[ind];
	BasisGroup *g = groups[gm[0]];
	return [g bladeAt:ind - gm[2]];
}

-(int) bladeIndex:(unsigned) mask
{
	if (![set is_member:mask]) {
		return 0;
	}
	return mask_map[mask - range[0]];
}

-(Set *) set
{
	return set;
}
@end
