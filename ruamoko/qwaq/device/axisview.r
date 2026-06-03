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

-(string)format
{
	return sprintf ("%d", axis.value);
}

-(int)rows
{
	return 1;
}

-(AxisView *)cellAtRow:(int)row forColumn:(TableColumn *)column
{
	if ([column name] == "axis") {
		return [NameView withName:sprintf("%d", axis.axis)];
	}
	return self;
}

@end
