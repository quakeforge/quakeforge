#ifndef TextureView_h
#define TextureView_h

#include <AppKit/AppKit.h>

@ interface TextureView:NSView {
	id          parent_i;
	int         deselectIndex;
}

-setParent:(id) from;
-deselect;

@end
#endif // TextureView_h
