#ifndef __qwaq_debugger_indexview_h
#define __qwaq_debugger_indexview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface IndexView : DefView
{
	int         index;
}
+(IndexView *)withIndex:(int)index;
@end

#endif//__qwaq_debugger_indexview_h
