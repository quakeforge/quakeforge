#ifndef __qwaq_debugger_nameview_h
#define __qwaq_debugger_nameview_h

#include "debugger/defview.h"

@interface NameView : DefView
{
	string     name;
}
+(NameView *)withName:(string)name;
@end

#endif//__qwaq_debugger_nameview_h
