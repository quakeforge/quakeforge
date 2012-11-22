void (obj_module_t [] msg) __obj_exec_class = #0;
id (id receiver, SEL op, ...) obj_msgSend = #0;

struct val_s {
	integer w,x,y,z;
};
typedef struct val_s val_t;

struct foo_s {
	integer a;
	val_t b;
};
typedef struct foo_s foo_t;

@interface Foo
{
	val_t v;
}
- (val_t) v;
@end
/*
@implementation Foo
- (val_t) v
{
	return v;
}
@end

val_t () foo =
{
//	local val_t v;
	local foo_t f;
	local foo_t []ff = &f;
//	v.x = v.y = v.z = v.w = 0;
//	f.b = v;
//	v = f.b;
//	return v;
//	ff.b = v;
//	v = ff.b;
	return ff.b;
}
*/
void (Foo f) bar =
{
//	local val_t v = foo ();
	local val_t u = [f v];
}
