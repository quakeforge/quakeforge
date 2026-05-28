#ifndef __qwaq_debugger_nameview_h
#define __qwaq_debugger_nameview_h

#include "ruamoko/qwaq/device/axisview.h"

@interface NameView : AxisView
{
	string     name;
}
+(NameView *)withName:(string)name;
@end

#endif//__qwaq_debugger_nameview_h
