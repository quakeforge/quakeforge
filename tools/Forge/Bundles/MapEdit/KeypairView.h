#ifndef KeypairView_h
#define KeypairView_h

#include <AppKit/AppKit.h>

extern id  keypairview_i;

@interface KeypairView: NSView
{
}

- (id) calcViewSize;

#define SPACING     4
#define FONTSIZE    12
#define EXTRASPC    2

#define LINEHEIGHT  16

@end

#endif // KeypairView_h
