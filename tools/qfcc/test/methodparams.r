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
