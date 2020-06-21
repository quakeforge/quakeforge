#ifndef __qwaq_debugger_vectorview_h
#define __qwaq_debugger_vectorview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface VectorView : DefView
{
	vector     *data;
}
+(VectorView *)withType:(qfot_type_t *)type at:(unsigned)offset in:(void *)data;
@end

#endif//__qwaq_debugger_vectorview_h
