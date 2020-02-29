#include "qwaq-window.h"

@implementation Window
+windowWithRect: (Rect) rect
{
	return [[[self alloc] initWithRect: rect] autorelease];
}
@end
