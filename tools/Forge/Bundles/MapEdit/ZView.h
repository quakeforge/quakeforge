#ifndef ZView_h
#define ZView_h

#include <AppKit/AppKit.h>

#include "QF/mathlib.h"

#include "render.h"

extern id  zview_i;

// zplane controls the objects displayed in the xyview
extern float    zplane;
extern float    zplanedir;

@interface ZView: NSView
{
	float   minheight, maxheight;
	float   oldminheight, oldmaxheight;
	float   topbound, bottombound;  // for floor clipping

	float  scale;

	vec3_t  origin;

	NSBezierPath  *checker;
	NSFont      *font;
}

- (id) clearBounds;
- (id) getBounds: (float *)top
   : (float *)bottom;

- (id) getPoint: (NSPoint *)pt;
- (id) setPoint: (NSPoint *)pt;

- (id) addToHeightRange: (float)height;

- (id) newRealBounds;
- (id) newSuperBounds;

- (void) XYDrawSelf;

- (BOOL) XYmouseDown: (NSPoint *)pt;

- (id) setXYOrigin: (NSPoint *)pt;

- (id) setOrigin: (NSPoint)pt scale: (float)sc;

@end
#endif // ZView_h
