#ifndef __ruamoko_gui_Point_h
#define __ruamoko_gui_Point_h

#include "Object.h"

@interface Point: Object
{
@public
	integer	x;
	integer	y;
}

- (id) initWithComponents: (integer)_x : (integer)_y;
- (id) initWithPoint: (Point)aPoint;
- (id) copy;

- (void) addPoint: (Point)aPoint;
- (void) subtractPoint: (Point)aPoint;

- (integer) x;
- (integer) y;

- (void) setPoint: (Point)aPoint;
@end

#endif //__ruamoko_gui_Point_h
