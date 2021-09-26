#include <string.h>
#include "ruamoko/qwaq/input-app.h"
#include "ruamoko/qwaq/ui/scrollbar.h"
#include "ruamoko/qwaq/ui/stringview.h"
#include "ruamoko/qwaq/ui/tableview.h"
#include "ruamoko/qwaq/ui/window.h"
#include "ruamoko/qwaq/device/axisdata.h"
#include "ruamoko/qwaq/device/device.h"

@implementation Device

-initWithDevice:(qwaq_devinfo_t *)device id:(int)devid
{
	if (!(self = [super init])) {
		return nil;
	}
	self.device = device;
	self.devid = devid;

	window = [Window withRect:{{0, 0}, {40, 10}}];
	[window setBackground: color_palette[064]];
	[window setTitle: device.id];

	[window insert:[StringView withRect:{{1, 1}, {38, 1}} string:device.name]];
	axis_data = [[AxisData withDevice:device] retain];
	axis_view = [TableView withRect:{{1, 2}, {38, 7}}];
	[axis_view addColumn:[TableViewColumn named:"axis" width:3]];
	[axis_view addColumn:[TableViewColumn named:"value" width:6]];
	ScrollBar *sb = [ScrollBar vertical:7 at:{39, 2}];
	[axis_view setVerticalScrollBar:sb];
	[axis_view setDataSource:axis_data];
	[window insertSelected: axis_view];
	[window insert: sb];
	[application addView: window];

	return self;
}

+withDevice:(qwaq_devinfo_t *)device id:(int)devid
{
	return [[[self alloc] initWithDevice:device id:devid] autorelease];
}

-(void)dealloc
{
	[application removeView:window];
	[axis_data release];

	obj_free (device.axes);
	obj_free (device.buttons);
	str_free (device.name);
	str_free (device.id);
	obj_free (device);
	[super dealloc];
}

-updateAxis:(int)axis value:(int)value
{
	[axis_data updateAxis:axis value:value];
	return self;
}

-updateButton:(int)button state:(int)state
{
	return self;
}

-(int)devid
{
	return devid;
}

-(string)name
{
	return device.name;
}

-(string)id
{
	return device.id;
}

-redraw
{
	[axis_view redraw];
	return self;
}

@end
