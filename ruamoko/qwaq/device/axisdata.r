#include "ruamoko/qwaq/device/axisdata.h"
#include "ruamoko/qwaq/device/axisview.h"

@implementation AxisData

-initWithDevice:(qwaq_devinfo_t *)device
{
	if (!(self = [super init])) {
		return nil;
	}
	numaxes = device.numaxes;
	axes = device.axes;
	axis_views = obj_malloc (numaxes);
	for (int i = 0; i < numaxes; i++) {
		axis_views[i] = [[AxisView withAxis:&axes[i]] retain];
	}
	return self;
}

+withDevice:(qwaq_devinfo_t *)device
{
	return [[[self alloc] initWithDevice:device] autorelease];
}

-(void)dalloc
{
}

-updateAxis:(int)axis value:(int)value
{
	axes[axis].value = value;
	return self;
}

-(ListenerGroup *)onRowCountChanged
{
	return nil;
}

-(int)numberOfRows:(TableView *)tableview
{
	return numaxes;
}

-(View *)tableView:(TableView *)tableview
		 forColumn:(TableViewColumn *)column
			   row:(int)row
{
	View       *view = nil;

	if (row >= 0 && row < numaxes) {
		AxisView   *av = axis_views[row];
		view = [av viewAtRow:0 forColumn:column];
	}
	return view;
}

@end
