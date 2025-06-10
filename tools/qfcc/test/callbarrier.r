#include "test-harness.h"

typedef struct Point {
	int x;
	int y;
} Point;

typedef struct Size {
	int width;
	int height;
} Size;

typedef struct Rect {
	Point   origin;
	Size    size;
} Rect;

Rect make_rect (int x, int y, int w, int h);
void *create (int h, int w);

bool check_call (Rect rect)
{
	auto r = make_rect (rect.origin.x, rect.origin.y, rect.size.width * 8, 8);
	auto p = create (rect.size.height, rect.size.width);
	return (r.origin.x == rect.origin.x && r.origin.y == rect.origin.y
			&& r.size.width == rect.size.width * 8 && r.size.height == 8);
}

Rect make_rect (int x, int y, int w, int h)
{
	return {{x, y}, {w, h}};
}

void *create (int h, int w)
{
	return nil;
}

int main ()
{
	return check_call ({{1, 2}, {3, 4}}) ? 0 : 1;
}
