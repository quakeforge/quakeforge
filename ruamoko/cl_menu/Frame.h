#include "gui/Size.h"
#include "draw.h"

@interface Frame : Object
{
	QPic *picture;
	float duration;
}
- (id) initWithFile: (string) file duration: (float) time;
- (void) dealloc;
- (Size) size;
- (float) duration;
- (void) draw: (integer) x :(integer) y;
@end
