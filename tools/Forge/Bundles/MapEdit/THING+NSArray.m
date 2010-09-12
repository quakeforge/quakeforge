@implementation THING (NSArray)

- (NSUInteger) count
{
	return [array count];
}

- (id) objectAtIndex: (NSUInteger)index
{
	if (index >= [self count])
		return 0;
	return [array objectAtIndex: index];
}

- (void) addObject: (id)anObject
{
	[array addObject: anObject];
}

- (void) insertObject: (id)anObject atIndex: (NSUInteger)index
{
	[array insertObject: anObject atIndex: index];
}

- (void) removeObjectAtIndex: (NSUInteger)index
{
	[array removeObjectAtIndex: index];
}

- (void) replaceObjectAtIndex: (NSUInteger)index withObject: (id)anObject
{
	[array replaceObjectAtIndex: index withObject: anObject];
}

@end
