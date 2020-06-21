#ifndef __qwaq_ui_proxyview_h
#define __qwaq_ui_proxyview_h

#include "ruamoko/qwaq/ui/view.h"

@class Group;

@interface ProxyView : Object
{
	View       *view;
	Group      *owner;
}
+(ProxyView *)withView:(View *)view;
-initWithView:(View *)view;
-setView:(View *)view;
@end

@interface ProxyView (View) <View, TextContext>
@end

#endif//__qwaq_ui_proxyview_h
