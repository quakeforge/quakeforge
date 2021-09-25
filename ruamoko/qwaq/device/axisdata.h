#ifndef __qwaq_device_axisdata_h
#define __qwaq_device_axisdata_h

#include "ruamoko/qwaq/qwaq-input.h"
#include "ruamoko/qwaq/ui/tableview.h"

@class AxisView;

@interface AxisData : Object <TableViewDataSource>
{
	int         numaxes;
	in_axisinfo_t *axes;
	AxisView  **axis_views;
}
+withDevice:(qwaq_devinfo_t *)device;
-updateAxis:(int)axis value:(int)value;
@end

#endif//__qwaq_device_axisdata_h
