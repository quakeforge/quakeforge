#ifndef __qwaq_debugger_floatview_h
#define __qwaq_debugger_floatview_h

#include "debugger/defview.h"

@interface FloatView : DefView
{
	float      *data;
}
+(FloatView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_floatview_h
