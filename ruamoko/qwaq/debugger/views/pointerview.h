#ifndef __qwaq_debugger_pointerview_h
#define __qwaq_debugger_pointerview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface PointerView : DefView
{
	unsigned   *data;
	bool        invalid;
	unsigned    ptr;
	qfot_type_t *ptr_type;
	int         ptr_size;
	void       *ptr_data;
	DefView    *ptr_view;
}
+(PointerView *)withDef:(qdb_def_t)def type:(qfot_type_t *)type in:(void *)data target:(qdb_target_t)target;
@end

#endif//__qwaq_debugger_pointerview_h
