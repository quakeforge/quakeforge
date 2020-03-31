#ifndef __qwaq_debugger_doubleview_h
#define __qwaq_debugger_doubleview_h

#include "debugger/defview.h"

@interface DoubleView : DefView
{
	double     *data;
}
+(DoubleView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_doubleview_h
