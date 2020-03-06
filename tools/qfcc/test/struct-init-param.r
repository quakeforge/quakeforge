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

void *foo (void *obj, void *cmd, void *o, Point pos, Rect r)
{
	return obj;
}

void *bar (Rect *obj, void *cmd, void *o, Point pos)
{
	Rect rect = { {}, obj.extent };
	return foo (obj, cmd, o, pos, rect);
}

int main (void)
{
	return 1;// don't want this to pass until I've checked the generated code
}
