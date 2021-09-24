#ifndef __qwaq_ui_stringview_h
#define __qwaq_ui_stringview_h

#include "ruamoko/qwaq/ui/view.h"

@interface StringView : View
{
	string      str;
}
+(StringView *)withRect:(Rect)rect;
+(StringView *)withRect:(Rect)rect string:(string)str;
-setString:(string)newString;
@end

#endif//__qwaq_ui_stringview_h
