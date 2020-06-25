#ifndef __qwaq_debugger_uintview_h
#define __qwaq_debugger_uintview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface UIntView : DefView
{
	unsigned   *data;
}
+(UIntView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_uintview_h
