#ifndef PopScrollView_h
#define PopScrollView_h

#include <AppKit/AppKit.h>

@interface PopScrollView: NSScrollView
{
	id  button1, button2;
}

- (id) initWithFrame: (NSRect)frameRect
   button1: b1
   button2: b2;

- (id) tile;

@end
#endif // PopScrollView_h
