#pragma bug die
#pragma warn error
#pragma optimize on

void str_free(string str)
{
}

@interface Object
{
	Class		isa;
}
-(void)dealloc;
@end

@interface derived : Object
{
	int  		free;
	string      foo;
	id          bar;
}
@end

@implementation Object
-(void) dealloc
{
	// this is the root of the hierarchy, so no super to call, thus
	// must not check for [super dealloc]
}
-(void) release
{
}
@end

@implementation derived
-(void) dealloc
{
	// as this is a derived class, failure to call [super dealloc] will
	// result in a memory leak (yes, there could be special allocators
	// involved, in which case something will be needed to inform the
	// compiler)
	str_free (foo);
}
@end

void __obj_exec_class (struct obj_module *msg) = #0;
id obj_msgSend_super (Super *class, SEL op, ...) = #0;

int main ()
{
	return 1;	// test fails if compile succeeds (with -Werror)
}
