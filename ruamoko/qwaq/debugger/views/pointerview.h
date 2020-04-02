#ifndef __qwaq_debugger_pointerview_h
#define __qwaq_debugger_pointerview_h

#include "debugger/views/defview.h"

@interface PointerView : DefView
{
	unsigned   *data;
}
+(PointerView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_pointerview_h
