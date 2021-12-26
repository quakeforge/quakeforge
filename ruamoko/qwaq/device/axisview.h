#ifndef __qwaq_device_axisview_h
#define __qwaq_device_axisview_h

#include "ruamoko/qwaq/qwaq-input.h"
#include "ruamoko/qwaq/ui/view.h"

@class TableViewColumn;

@interface AxisView : View
{
	in_axisinfo_t *axis;
}
+withAxis:(in_axisinfo_t *)axis;
-(int)rows;
-(View *)viewAtRow:(int)row forColumn:(TableViewColumn *)column;
@end

#endif//__qwaq_device_axisview_h
