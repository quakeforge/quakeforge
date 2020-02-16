#pragma bug die

struct Point {
	int x;
	int y;
};
typedef struct Point Point;

@interface Object
{
	int   foo;
	Point origin;
}
-(Point) origin;
@end

@implementation Object
-(Point) origin
{
	return origin;
}
@end
void __obj_exec_class (struct obj_module *msg) = #0;
int main()
{
	return 0;	// to survive and prevail
}
