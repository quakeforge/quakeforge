#ifndef __qwaq_debugger_defview_h
#define __qwaq_debugger_defview_h

#include <types.h>
#include "ruamoko/qwaq/ui/view.h"
#include "ruamoko/qwaq/debugger/debug.h"

@interface DefView : View
{
	qfot_type_t *type;
	qdb_target_t target;
}
+(DefView *)withType:(qfot_type_t *)type
				  at:(unsigned)offset
				  in:(void *)data
			  target:(qdb_target_t)target;
-initWithType:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_defview_h
