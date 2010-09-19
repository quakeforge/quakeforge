#ifndef ZView_h
#define ZView_h

#include <AppKit/AppKit.h>

#include "QF/mathlib.h"

#include "render.h"

extern id   zview_i;

// zplane controls the objects displayed in the xyview
extern float zplane;
extern float zplanedir;

@interface ZView:NSView
{
	float       minheight, maxheight;
	float       oldminheight, oldmaxheight;
	float       topbound, bottombound;	// for floor clipping

	float       scale;

	vec3_t      origin;
}

-clearBounds;
-getBounds: (float *) top:(float *) bottom;

-getPoint:(NSPoint *) pt;
-setPoint:(NSPoint *) pt;

-addToHeightRange:(float) height;

-newRealBounds;
-newSuperBounds;

-XYDrawSelf;

-(BOOL) XYmouseDown:(NSPoint *) pt;

-setXYOrigin:(NSPoint *) pt;

-setOrigin:(NSPoint) pt scale:(float) sc;

@end
#endif // ZView_h
