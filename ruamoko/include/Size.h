#ifndef __ruamoko_Size_h
#define __ruamoko_Size_h

#include "Object.h"
#include "Point.h"
#include "Size.h"

@interface Size: Object
{
@public
	integer	width;
	integer	height;
}

- (id) initWithWidth: (integer)w height: (integer)h;
- (id) initWithSize: (Size)aSize;
- (id) copy;

- (void) addSize: (Size)aSize;
- (void) subtractSize: (Size)aSize;

- (integer) width;
- (integer) height;

- (void) setSize: (Size)aSize;
- (void) setWidth: (integer)_width;
- (void) setHeight: (integer)_width;

@end

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

- (Point) origin;
- (Size) size;

- (void) setSize: (Size)aSize;

@end

#endif//__ruamoko_point_h
