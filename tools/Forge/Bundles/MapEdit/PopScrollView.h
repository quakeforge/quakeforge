#ifndef PopScrollView_h
#define PopScrollView_h

#include <AppKit/AppKit.h>

@interface PopScrollView : NSScrollView
{
	id	button1, button2;
}

- initWithFrame:(NSRect)frameRect button1: b1 button2: b2;
- tile;

@end

#endif//PopScrollView_h
