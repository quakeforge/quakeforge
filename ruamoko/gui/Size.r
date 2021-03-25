#include <gui/Size.h>

Size makeSize (int width, int height)
{
	Size s = {width, height};
	return s;
}

Size addSize (Size a, Size b)
{
	Size c = {a.width + b.width, a.height + b.height};
	return c;
}

Size subtractSize (Size a, Size b)
{
	Size c = {a.width - b.width, a.height - b.height};
	return c;
}
