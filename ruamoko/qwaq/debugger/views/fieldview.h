#ifndef __qwaq_debugger_fieldview_h
#define __qwaq_debugger_fieldview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface FieldView : DefView
{
	unsigned   *data;
}
+(FieldView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_fieldview_h
