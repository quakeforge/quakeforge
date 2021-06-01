#ifndef __qwaq_debugger_vectorview_h
#define __qwaq_debugger_vectorview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface VectorView : DefView
{
	vector     *data;
}
+(VectorView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_vectorview_h
