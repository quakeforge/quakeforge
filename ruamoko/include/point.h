#ifndef __ruamoko_point_h
#define __ruamoko_point_h

#include "object.h"

@interface Point : Object
{
@public
	integer	x,y;
}
-initAtX:(integer)_x Y:(integer)_y;
-initWithPoint:(Point)p;
-setTo:(Point)p;
-moveBy:(Point)p;
@end

#endif//__ruamoko_point_h
