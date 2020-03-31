#ifndef __qwaq_debugger_intview_h
#define __qwaq_debugger_intview_h

#include "debugger/defview.h"

@interface IntView : DefView
{
	int        *data;
}
+(IntView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_intview_h
