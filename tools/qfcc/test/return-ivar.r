#pragma bug die
typedef struct Point {
	int x;
	int y;
} Point;

@interface Object	//just so the runtime doesn't complain
{
	Class isa;
	Point origin;
}
-(Point) origin;
@end
@implementation Object
-(Point) origin
{
	Point p;
	p = origin;
	return p;
}
@end
void __obj_exec_class (struct obj_module *msg) = #0;

int main()
{
	return 0;
}
