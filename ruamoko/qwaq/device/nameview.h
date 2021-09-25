#ifndef __qwaq_debugger_nameview_h
#define __qwaq_debugger_nameview_h

#include "ruamoko/qwaq/debugger/views/defview.h"

@interface NameView : View
{
	string     name;
}
+(NameView *)withName:(string)name;
@end

#endif//__qwaq_debugger_nameview_h
