#pragma bug die
void __obj_exec_class (struct obj_module *msg) = #0;
id obj_msgSend_super (Super *class, SEL op, ...) = #0;
id (id receiver, SEL op, ...) obj_msgSend = #0;
id (Class class) class_create_instance = #0;
int obj_increment_retaincount (id object) = #0;
void __obj_forward(id object, SEL sel, ...) = #0;

int func (SEL foo, @va_list bar);

@interface Object   //just so the runtime doesn't complain
{
	Class isa;
}
+alloc;
-init;
-(void) forward: (SEL) sel : (@va_list) args;
@end

@interface Foo : Object
-(int)dosomething;
@end

@implementation Object
-(void) forward: (SEL) sel : (@va_list) args
{
	@return func (sel, args);
}
+alloc
{
	return class_create_instance (self);
}

-init
{
	obj_increment_retaincount (self);
	return self;
}
@end

int func (SEL foo, @va_list bar)
{
	return 42;
}

int
main ()
{
	id obj = [[Object alloc] init];
	return !([obj dosomething] == 42);
}
