void printf (string fmt, ...) = #0;
typedef struct {
	int         x;
	int         y;
} Point;

typedef struct {
	int         width;
	int         height;
} Extent;

typedef struct Rect_s {
	Point       offset;
	Extent      extent;
} Rect;

void *foo (Rect *obj, void *cmd, Rect *o, Point pos, Rect r);

void *baz (Rect *obj, void *cmd, Rect *o, Point pos)
{
	Rect rect = { {1, 2}, {3, 4} };
	return foo (obj, cmd, o, pos, rect);
}

void *bar (Rect *obj, void *cmd, Rect *o, Point pos)
{
	Rect rect = { {}, obj.extent };
	return foo (obj, cmd, o, pos, rect);
}

void *foo (Rect *obj, void *cmd, Rect *o, Point pos, Rect r)
{
	*o = r;
	return obj;
}

Rect obj = { { 1, 2}, { 3, 4} };
Rect o = { { 5, 6}, {7, 8} };

int main (void)
{
	int         ret = 0;
	bar(&obj, nil, &o, obj.offset);
	printf ("%d %d %d %d\n", o.offset.x, o.offset.y,
			o.extent.width, o.extent.height);
	if not (o.offset.x == 0 && o.offset.y == 0
		&& o.extent.width == 3 && o.extent.height == 4)
		ret |= 1;

	baz(&obj, nil, &o, obj.offset);
	printf ("%d %d %d %d\n", o.offset.x, o.offset.y,
			o.extent.width, o.extent.height);
	if not (o.offset.x == 1 && o.offset.y == 2
		&& o.extent.width == 3 && o.extent.height == 4)
		ret |= 1;
	return ret;
}
