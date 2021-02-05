#ifndef __ruamoko_gui_Rect_h
#define __ruamoko_gui_Rect_h

#include <gui/Point.h>
#include <gui/Size.h>

/**	\addtogroup gui */
///@{

struct Rect {
	Point	origin;
	Size	size;
};
typedef struct Rect Rect;

@extern Rect makeRect (int x, int y, int w, int h);
@extern Rect makeRectFromOriginSize (Point origin, Size size);

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

///@}

#endif //__ruamoko_gui_Rect_h
