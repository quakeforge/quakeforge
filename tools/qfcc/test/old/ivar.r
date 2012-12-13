@interface foo
{
	Class       isa;
	unsigned    retainCount;
}
+(id) alloc;
-(id) init;
@end

@interface baz: foo
{
	integer     xpos, ypos;
	integer     xlen, ylen;
	integer     xabs, yabs;
	baz        *parent;
	integer     flags;
}
@end

@class baz;
@class foo;

@interface bar: baz
{
	integer     blah;
}
-(id) init;
@end

@implementation bar
{
	integer     blah;
}

-(id) init
{
	blah = 0xdeadbeaf;
	return self;
}
@end
