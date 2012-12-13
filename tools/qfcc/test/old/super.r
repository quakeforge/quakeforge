void __obj_exec_class (obj_module_t *msg) = #0;

@interface Foo
-init;
@end

@interface Bar : Foo
@end

@implementation Bar
-init
{
	return [super init];
}
