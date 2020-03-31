#ifndef __qwaq_debugger_stringview_h
#define __qwaq_debugger_stringview_h

#include "debugger/defview.h"

@interface StringView : DefView
{
	string     *data;
}
+(StringView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_stringview_h
