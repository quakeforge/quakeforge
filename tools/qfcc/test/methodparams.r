#pragma bug die
typedef struct { int x, y; } Point;
@interface TextContext
- (void) mvvprintf: (Point) pos, string mft, @va_list args;
@end
@interface View
{
	TextContext *textContext;
}
- (void) mvprintf: (Point) pos, string mft, ...;
@end

@implementation View
- (void) mvprintf: (Point) pos, string fmt, ...
{
	[textContext mvvprintf: pos, fmt, @args];
}
@end
@attribute(no_va_list) id obj_msgSend (id receiver, SEL op, ...) = #0;
void __obj_exec_class (struct obj_module *msg) = #0;
@interface Object
@end
@implementation Object
@end

int
main (void)
{
	return 0;		// to survive and prevail :)
}
