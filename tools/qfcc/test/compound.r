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

int test_simple_param (Point p, int x, int y)
{
	int ret = !(p.x == x && p.y == y);
	if (ret) {
		printf ("simple param: {%d, %d} != {%d, %d}\n", p.x, p.y, x, y);
	}
	return ret;
}

int test_nested_param (Rect r, int x, int y, int w, int h)
{
	int ret = !(r.origin.x == x && r.origin.y == y
				&& r.size.width == w && r.size.height == h);
	if (ret) {
		printf ("nested param: {{%d, %d}, {%d, %d}} != ",
				r.origin.x, r.origin.y, r.size.width, r.size.height);
		printf ("{{%d, %d}, {%d, %d}}\n", x, y, w, h);
	}
	return ret;
}

int test_simple_assign (int x, int y)
{
	Point p;
	p = { x, y };
	int ret = !(p.x == x && p.y == y);
	if (ret) {
		printf ("simple assign: {%d, %d} != {%d, %d}\n", p.x, p.y, x, y);
	}
	return ret;
}

int test_nested_assign (int x, int y, int w, int h)
{
	Rect r;
	r = {{x, y}, {w, h}};
	int ret = !(r.origin.x == x && r.origin.y == y
				&& r.size.width == w && r.size.height == h);
	if (ret) {
		printf ("nested assign: {{%d, %d}, {%d, %d}} != ",
				r.origin.x, r.origin.y, r.size.width, r.size.height);
		printf ("{{%d, %d}, {%d, %d}}\n", x, y, w, h);
	}
	return ret;
}

int main (void)
{
	int ret = 0;
	ret |= test_simple_param ({1, 2}, 1, 2);
	ret |= test_nested_param ({{1, 2}, {3, 4}}, 1, 2, 3, 4);
	ret |= test_simple_assign (1, 2);
	ret |= test_nested_assign (1, 2, 3, 4);
	return ret;
}
