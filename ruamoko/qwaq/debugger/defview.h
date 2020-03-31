#ifndef __qwaq_debugger_defview_h
#define __qwaq_debugger_defview_h

#include <types.h>
#include "ui/view.h"

@interface DefView : View
{
	qfot_type_t *type;
}
+(DefView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
-initWithType:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_defview_h
