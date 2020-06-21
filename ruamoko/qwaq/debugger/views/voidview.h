#ifndef __qwaq_debugger_voidview_h
#define __qwaq_debugger_voidview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface VoidView : DefView
{
	unsigned   *data;
}
+(VoidView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_voidview_h
