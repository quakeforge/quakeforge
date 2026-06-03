#ifndef __qwaq_debugger_classructview_h
#define __qwaq_debugger_classructview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

// tread target pointers as handles
typedef @handle classptr_h classptr_t;

@interface ClassView : DefView
{
	classptr_t *data;			// referenced class
	classptr_t  type_class;		// actual class (only self and sub are valid)
	bvec2       valid;			//x = in heirarcy y = possibly legit
	classptr_t  instance_class;	// class current ivars are valid for
	unsigned    instance_name;	// class name for current ivars
	unsigned    instance_size;	// XXX in bytes (FIXME use bytes for get_data?)
	void       *instance_data;
	int         max_ivars;
	int         ivar_count;
	DefView   **ivar_views;
	int        *ivar_rows;
}
+(DefView *)withDef:(qdb_def_t)def in:(void *)data type:(qfot_type_t *)type target:(qdb_target_t)target;
@end

#endif//__qwaq_debugger_classructview_h
