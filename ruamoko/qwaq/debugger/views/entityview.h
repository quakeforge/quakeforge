#ifndef __qwaq_debugger_entityview_h
#define __qwaq_debugger_entityview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface EntityView : DefView
{
	entity     *data;
}
+(EntityView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_entityview_h
