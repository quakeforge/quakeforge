#ifndef __ruamoko_Rect_h
#define __ruamoko_Rect_h

#include "Object.h"
#include "gui/Point.h"
#include "gui/Size.h"

@interface Rect: Object
{
@public
	Point	origin;
	Size	size;
}

- (id) initWithComponents: (integer)x : (integer)y : (integer)w : (integer)h;
- (id) initWithOrigin: (Point)anOrigin size: (Size)aSize;
- (id) initWithRect: (Rect)aRect;
- (id) copy;

#if 0
- (BOOL) intersectsRect: (Rect)aRect;
- (BOOL) containsPoint: (Point)aPoint;
- (BOOL) containsRect: (Rect)aRect;
- (BOOL) isEqualToRect: (Rect)aRect;
- (BOOL) isEmpty;

- (Rect) intersectionWithRect: (Rect)aRect;
- (Rect) unionWithRect: (Rect)aRect;

- (Rect) insetBySize: (Size)aSize;
- (Rect) offsetBySize: (Size)aSize;
#endif

- (Point) origin;
- (Size) size;

- (void) setOrigin: (Point)aPoint;
- (void) setSize: (Size)aSize;
- (void) setRect: (Rect)aRect;

@end

#endif //__ruamoko_Rect_h
