#ifndef __qwaq_debugger_fieldview_h
#define __qwaq_debugger_fieldview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface FieldView : DefView
{
	unsigned   *data;
}
+(FieldView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_fieldview_h
