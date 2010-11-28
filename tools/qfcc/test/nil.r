Class x = NIL;

struct Size {
	integer x;
	integer y;
};
typedef struct Size Size;

@interface foo
-(Size) bar;
@end
@implementation foo
-(Size) bar
{
	local Size s;
	return NIL;
}
@end
