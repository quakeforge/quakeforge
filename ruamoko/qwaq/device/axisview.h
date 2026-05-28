#ifndef __qwaq_device_axisview_h
#define __qwaq_device_axisview_h

#include "ruamoko/qwaq/qwaq-input.h"
#include "ruamoko/qwaq/ui/tableview.h"

@interface AxisView : Object <TableViewCell>
{
	in_axisinfo_t *axis;
}
+withAxis:(in_axisinfo_t *)axis;
-(int)rows;
-(AxisView *)cellAtRow:(int)row forColumn:(TableViewColumn *)column;
@end

#endif//__qwaq_device_axisview_h
