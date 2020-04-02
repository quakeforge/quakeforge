#ifndef __qwaq_debugger_basicview_h
#define __qwaq_debugger_basicview_h

#include "debugger/views/defview.h"

@interface BasicView : DefView
// might return a NameView (which is also a DefView)
+(DefView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_basicview_h
