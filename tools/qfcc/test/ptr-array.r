@interface foo
{
	unsigned count;
	id *_objs;
}
@end

@implementation foo
- (void) foofoo: (id) anObject
{
	_objs[count++] = anObject;
}
@end
