#ifndef __ruamoko_Rect_h
#define __ruamoko_Rect_h

#include "Object.h"
#include "Point.h"
#include "Size.h"

@interface Rect: Object
{
@public
	Point	origin;
	Size	size;
}

- (id) initWithOrigin: (Point)_origin size: (Size)_size;
- (id) initWithComponents: (integer)x : (integer)y : (integer)w : (integer)h;
- (id) initWithRect: (Rect)aRect;
- (id) copy;

- (BOOL) intersectsRect: (Rect)aRect;
- (BOOL) containsPoint: (Point)aPoint;
- (BOOL) containsRect: (Rect)aRect;
- (BOOL) isEqualToRect: (Rect)aRect;
- (BOOL) isEmpty;

- (Rect) intersectionWithRect: (Rect)aRect;
- (Rect) unionWithRect: (Rect)aRect;

- (Rect) insetBySize: (Size)aSize;
- (Rect) offsetBySize: (Size)aSize;

- (Point) origin;
- (Size) size;

- (void) setSize: (Size)aSize;

@end

#endif //__ruamoko_Rect_h
