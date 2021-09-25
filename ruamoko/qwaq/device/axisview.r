#include <string.h>
#include "ruamoko/qwaq/ui/tableview.h"
#include "ruamoko/qwaq/device/axisview.h"
#include "ruamoko/qwaq/device/nameview.h"

@implementation AxisView

-initWithAxis:(in_axisinfo_t *)axis
{
	if (!(self = [super init])) {
		return nil;
	}
	self.axis = axis;
	return self;
}

+withAxis:(in_axisinfo_t *)axis
{
	return [[[self alloc] initWithAxis:axis] retain];
}

-draw
{
	[super draw];
	[self mvprintf:{0, 0}, "%*.*d", xlen, xlen, axis.value];
	return self;
}

-(int)rows
{
	return 1;
}

-(View *)viewAtRow:(int)row forColumn:(TableViewColumn *)column
{
	if ([column name] == "axis") {
		return [NameView withName:sprintf("%d", axis.axis)];
	}
	return self;
}

@end
