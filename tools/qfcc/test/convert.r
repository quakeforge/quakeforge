#include "gui/Rect.h"

void (integer x) foo;

void (float y) bar =
{
	foo (y);
	[[Rect alloc] initWithComponents:1 :2 :y :3];
};
