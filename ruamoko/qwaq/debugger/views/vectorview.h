#ifndef __qwaq_debugger_vectorview_h
#define __qwaq_debugger_vectorview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface VectorView : DefView
{
	vector     *data;
}
+(VectorView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target;
@end

#endif//__qwaq_debugger_vectorview_h
