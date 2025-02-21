#pragma bug die
@interface Object
{
	Class		isa;
}
-(void)dealloc;
@end

@interface derived : Object
{
	int  		free;
}
@end

@implementation Object
-(void) dealloc
{
	// this is the root of the hierarchy, so no super to call, thus
	// must not check for [super dealloc]
}
@end

void __obj_exec_class (struct obj_module *msg) = #0;
id obj_msgSend_super (Super *class, SEL op, ...) = #0;

@implementation derived
-(void) dealloc
{
	// as this is a derived class, failure to call [super dealloc] will
	// result in a memory leak (yes, there could be special allocators
	// involved, in which case something will be needed to inform the
	// compiler)
	if (free) {
		[super dealloc];
	} else {
		[super dealloc];
	}
}
@end

int main ()
{
	return 0;	// test passes if compile succeeds (with -Werror)
}
