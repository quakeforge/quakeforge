#ifndef __ProxyView_h
#define __ProxyView_h

#include "gui/View.h"

@interface ProxyView : View
{
	View title;
	View view;
}
-(id)initWithBounds:(Rect)aRect title:(View)aTitle view:(View)aView;
@end

#endif//__ProxyView_h
