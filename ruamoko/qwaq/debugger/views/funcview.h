#ifndef __qwaq_debugger_funcview_h
#define __qwaq_debugger_funcview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface FuncView : DefView
{
	unsigned   *data;
}
+(FuncView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_funcview_h
