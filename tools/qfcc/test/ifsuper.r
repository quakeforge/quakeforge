#pragma advanced
#include "test-harness.h"

int obj_increment_retaincount (id object) = #0;
int obj_get_retaincount (id object) = #0;
id (Class class) class_create_instance = #0;
id obj_msgSend_super (Super *class, SEL op, ...) = #0;

@interface Object
{
	Class       isa;
}
+(id) alloc;
-(id) init;
@end

@interface Foo : Object
-(id) init;
@end

@implementation Object
+(id) alloc
{
	return class_create_instance (self);
}
-(id) init
{
	obj_increment_retaincount (self);
	return self;
}
@end

@implementation Foo
-(id) init
{
	if (!(self = [super init])) {
		return nil;
	}
	return self;
}
@end

int main ()
{
	Foo        *foo = [[Foo alloc] init];
	if (!foo) {
		printf ("foo is nil\n");
		return 1;
	}
	int         retain = obj_get_retaincount (foo);
	if (retain != 1) {
		printf ("retain count != 1: %d\n", retain);
		return 1;
	}
	return 0;
}
