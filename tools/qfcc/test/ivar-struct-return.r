#pragma bug die
#include "test-harness.h"

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
+(Point) origin;
@end

@implementation Object
+(Point) origin
{
	origin = {1, 2};
	return origin;
}
@end
int main()
{
	Point p = [Object origin];
	return !(p.x == 1 && p.y == 2);
}
