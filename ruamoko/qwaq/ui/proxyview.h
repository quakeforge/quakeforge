#ifndef __qwaq_ui_proxyview_h
#define __qwaq_ui_proxyview_h

#include "ui/view.h"

@class Group;

@interface ProxyView : Object
{
	View       *view;
	Group      *owner;
}
-initWithView:(View *) view;
-setView: (View *) view;
@end

@interface ProxyView (View) <View, TextContext>
@end

#endif//__qwaq_ui_proxyview_h
