#ifndef ZScrollView_h
#define ZScrollView_h

#include <AppKit/AppKit.h>

@ interface ZScrollView:NSScrollView {
	id          button1;
}

-initWithFrame:(NSRect)
frameRect   button1:b1;

-tile;

@end
#endif // ZScrollView_h
