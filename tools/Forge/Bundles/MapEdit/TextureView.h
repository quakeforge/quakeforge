#ifndef TextureView_h
#define TextureView_h

#include <AppKit/AppKit.h>

@interface TextureView: NSView
{
	id      parent_i;
	int     deselectIndex;
}

- (id) setParent: (id)from;
- (id) deselect;

@end
#endif // TextureView_h
