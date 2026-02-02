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
+ (id) alloc;
- (id) init;
-(Point) origin;
@end

id (Class class) class_create_instance = #0;
int obj_increment_retaincount (id object) = #0;

@implementation Object
+ (id) alloc
{
    return class_create_instance (self);
}

- (id) init
{
	obj_increment_retaincount (self);
	return self;
}

-(Point) origin
{
	origin = {1, 2};
	return origin;
}
@end
int main()
{
	id obj = [[Object alloc] init];
	Point p = [obj origin];
	return !(p.x == 1 && p.y == 2);
}
