#include "gui/Point.h"
#include "draw.h"

@interface Frame : Object
{
	QPic picture;
	float duration;
}
- (id) initWithFile: (string) file duration: (float) time;
- (void) dealloc;
- (Point) size;
- (float) duration;
- (void) draw: (integer) x :(integer) y;
@end
