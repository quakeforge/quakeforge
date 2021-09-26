#ifndef __qwaq_device_device_h
#define __qwaq_device_device_h

#include "ruamoko/qwaq/qwaq-input.h"

@class Window;
@class TableView;
@class AxisData;

@interface Device : Object
{
	int         devid;
	qwaq_devinfo_t *device;

	Window     *window;
	TableView  *axis_view;
	AxisData   *axis_data;
}
+withDevice:(qwaq_devinfo_t *)device id:(int)devid;
-updateAxis:(int)axis value:(int)value;
-updateButton:(int)button state:(int)state;
-(int)devid;
-(string)name;
-(string)id;
-redraw;
@end

#endif//__qwaq_device_device_h
