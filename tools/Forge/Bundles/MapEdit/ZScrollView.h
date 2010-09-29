#ifndef ZScrollView_h
#define ZScrollView_h

#include <AppKit/AppKit.h>

@interface ZScrollView: NSScrollView
{
	id  button1;
}

- (id) initWithFrame: (NSRect)frameRect
   button1: b1;

- (id) tile;

@end
#endif // ZScrollView_h
