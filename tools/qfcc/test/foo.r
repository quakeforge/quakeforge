#include <Object.h>

@interface Foo : Object
-free;
@end

@implementation Foo
-free
{
	[super free];
	[self free];
}
@end
