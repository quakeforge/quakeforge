#ifndef __qwaq_proxyview_h
#define __qwaq_proxyview_h

#include "qwaq-view.h"

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

#endif//__qwaq_proxyview_h
