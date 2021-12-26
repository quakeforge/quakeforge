typedef struct Point_s {
	int         x;
	int         y;
} Point;

typedef struct Extent_s {
	int         width;
	int         height;
} Extent;

int
foo (Point p)
{
	Extent e = *(Extent *)&p;
	return e.width * e.height;
}

int
main (void)
{
	return foo ({ 6, 7 }) != 42;
}
