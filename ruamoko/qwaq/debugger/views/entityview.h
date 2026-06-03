#ifndef __qwaq_debugger_entityview_h
#define __qwaq_debugger_entityview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface EntityView : DefView
{
	entity     *data;
}
+(EntityView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target;
@end

#endif//__qwaq_debugger_entityview_h
