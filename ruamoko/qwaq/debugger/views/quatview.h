#ifndef __qwaq_debugger_quatview_h
#define __qwaq_debugger_quatview_h

#include "debugger/views/defview.h"

@interface QuatView : DefView
{
	quaternion *data;
}
+(QuatView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_quatview_h
