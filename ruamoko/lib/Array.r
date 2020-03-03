#include <math.h>

#include <Array.h>
#include <runtime.h>

#define STANDARD_CAPACITY 16
#define ARRAY_MAX_GRANULARITY 100

/*
	Optimization opportunity:

	if ([self class] == [Array class]) {
		[[do things optimally instead of only through primitive methods]]
	}
*/

@implementation Array

+ (id) array
{
	return [self arrayWithCapacity: STANDARD_CAPACITY];
}

+ (id) arrayWithCapacity: (unsigned)cap
{
	return [[[self alloc] initWithCapacity: cap] autorelease];
}

+ (id) arrayWithArray: (Array *)array
{
	return [[array copy] autorelease];
}

+ (id) arrayWithObject: (id)anObject
{
	Array *newArray = (Array *)[self arrayWithCapacity: STANDARD_CAPACITY];

	[newArray addObject: anObject];
	return newArray;
}

+ (id) arrayWithObjects: (id)firstObj, ...
{
	local int i;
	id	newArray = [self arrayWithObject: firstObj];

	for (i = 0; i < @args.count; i++) {
		[newArray addObject: (id) @args.list[i].pointer_val];
	}
	return [newArray autorelease];
}

+ (id) arrayWithObjects: (id *) objs count: (unsigned)cnt
{
	local int i;
	id	newArray = [self array];

	for (i = 0; i < cnt; i++) {
		[newArray addObject: (id) objs[i]];
	}
	return newArray;
}

- (id) init
{
	return [self initWithCapacity: STANDARD_CAPACITY];
}

- (id) initWithCapacity: (unsigned)cap
{
	if (!(self = [super init]))
		return nil;

	count = 0;
	if ((capacity = cap) < 1)
		capacity = 1;

	granularity = (capacity + 1) / 2;
	if (granularity < 1)
		granularity = 1;

	if (granularity > ARRAY_MAX_GRANULARITY)
		granularity = ARRAY_MAX_GRANULARITY;

	_objs = (id *) obj_malloc (capacity * @sizeof (id));
	return self;
}

- (id) initWithArray: (Array *)array
{
#if 0
	local unsigned i;
	local unsigned max = [array count];

	if (!(self = [self initWithCapacity: max]))
		return nil;

	for (i = 0; i < max; i++) {
		_objs[i] = [[array objectAtIndex: i] retain];
	}
	return self;
#else
	return [self initWithArray: array copyItems: NO];
#endif
}

- (id) initWithArray: (Array *)array
           copyItems: (BOOL)copy
{
	local unsigned i;
	local unsigned max = [array count];

	if (!(self = [self initWithCapacity: max]))
		return nil;

	for (i = 0; i < max; i++) {
		if (copy)
			_objs[i] = [[array objectAtIndex: i] copy];
		else
			_objs[i] = [[array objectAtIndex: i] retain];
	}
	return self;
}

- (id) initWithObjects: (id)firstObj, ...
{
	local int i;

	if (!(self = [self initWithCapacity: @args.count + 1]))
		return nil;

	[self addObject: firstObj];
	for (i = 0; i < @args.count; i++) {
		[self addObject: (id) @args.list[i].pointer_val];
	}
	return self;
}

- (id) initWithObjects: (id *) objs count: (unsigned)cnt
{
	local int i;

	if (!(self = [self initWithCapacity: cnt]))
		return nil;

	for (i = 0; i < cnt; i++) {
		[self addObject: (id) objs[i]];
	}
	return self;
}

- (BOOL) containsObject: (id)anObject
{
	return [self indexOfObject: anObject] ? YES : NO;
}

- (unsigned) count
{
	return count;
}

- (id) objectAtIndex: (unsigned)index
{
	if (index >= count) // FIXME: need exceptions
		[self error: "-replaceObjectAtIndex:withObject: index out of range"];

	return _objs[index];
}

- (id) lastObject
{
	return [self objectAtIndex: [self count] - 1];
}

/*
	Finding Objects
*/
- (unsigned) indexOfObject: (id)anObject
{
	local unsigned	i;

	for (i = 0; i < [self count]; i++) {
		if ([[self objectAtIndex: i] isEqual: anObject])
			return i;
	}
	return NotFound;
}

#if 0
- (unsigned) indexOfObject: (id)anObject
                   inRange: (Range)range;
{
	local unsigned	i;
	local unsigned	end = range.location + range.length;

	for (i = range.location; i < end && i < [self count]; i++) {
		if ([[self objectAtIndex: i] isEqual: anObject])
			return i;
	}
	return NotFound;
}
#endif

- (unsigned) indexOfObjectIdenticalTo: (id)anObject
{
	local unsigned	i;

	for (i = 0; i < [self count]; i++) {
		if ([self objectAtIndex: i] == anObject)
			return i;
	}
	return NotFound;
}

#if 0
- (unsigned) indexOfObjectIdenticalTo: (id)anObject
                              inRange: (Range)range;
{
	local unsigned	i;
	local unsigned	end = range.location + range.length;

	for (i = range.location; i < end && i < [self count]; i++) {
		if ([self objectAtIndex: i] == anObject)
			return i;
	}
	return NotFound;
}
#endif

/*
	Adding objects
*/

- (void) addObject: (id)anObject
{
	if (count == capacity) {
		capacity += granularity;
		_objs = (id *)obj_realloc (_objs, capacity * @sizeof (id));
	}
	_objs[count] = [anObject retain];
	count++;
}

- (void) addObjectsFromArray: (Array *)array
{
	local unsigned	i;

	if (!array) // FIXME: need exceptions
		[self error: "-addObjectsFromArray: passed nil argument"];

	if (array == self)	// FIXME: need exceptions
		[self error: "-addObjectsFromArray: tried to add objects from self"]; // FIXME: need exceptions

	for (i = 0; i < [array count]; i++) {
		[self addObject: [array objectAtIndex: i]];
	}
}

- (void) insertObject: (id)anObject
              atIndex: (unsigned)index
{
	local unsigned	i;

	if (index > count) // FIXME: need exceptions
		[self error: "-insertObject:atIndex: index out of range"];

	if (count == capacity) {	// at capacity, expand
		_objs = (id *)obj_realloc (_objs, capacity * @sizeof (id));
		capacity += granularity;
	}

	for (i = count; i > index; i--) {
		_objs[i] = _objs[i - 1];
	}

	_objs[index] = [anObject retain];
	count++;

	return;
}

/*
	Replacing objects
*/

- (void) replaceObjectAtIndex: (unsigned)index
                   withObject: (id)anObject
{
	local id tmp;

	//if (!anObject)	// FIXME: need exceptions
	//	[self error: "-replaceObjectAtIndex:withObject: passed nil object"];
	if (index >= count) // FIXME: need exceptions
		[self error: "-replaceObjectAtIndex:withObject: index out of range"];

	// retain before release
	tmp = _objs[index];
	_objs[index] = [anObject retain];
	[tmp release];
}

- (void) setArray: (Array *)array
{
	if (self == array)
		return;

	[self removeAllObjects];
	[self addObjectsFromArray: array];
}

/*
	Object removal
*/
- (void) removeAllObjects
{
#if 0
	local id	tmp;

	while (count) {
		/*
			We do it this way to avoid having something weird happen when
			the object is released (dealloc may trigger, which in turn could
			cause something else to happen).
		*/
		tmp = _objs[--count];
		_objs[i] = nil;
		[tmp release];
	}
#else
	while ([self count]) {
		[self removeLastObject];
	}
#endif
}

- (void) removeLastObject
{
	local id	tmp;

	tmp = _objs[--count];
	_objs[count] = nil;
	[tmp release];
}

- (void) removeObject: (id)anObject
{
	local unsigned	i = [self count];
	do {
		--i;
		if ([[self objectAtIndex: i] isEqual: anObject]) {
			[self removeObjectAtIndex: i];
		}
	} while (i);
}

- (void) removeObjectAtIndex: (unsigned)index
{
	local int	i;
	local id		temp;

	if (index >= count) // FIXME: need exceptions
		[self error: "-removeObjectAtIndex: index out of range"];

	temp = _objs[index];
	count--;
	for (i = index; i < count; i++) {	// reassign all objs >= index
		_objs[i] = _objs[i+1];
	}

	[temp release];
}

- (void) removeObjectIdenticalTo: (id)anObject
{
	local unsigned	i = [self count];
	do {
		--i;
		if ([self objectAtIndex: i] == anObject) {
			[self removeObjectAtIndex: i];
		}
	} while (i);
}

- (void) removeObjectsInArray: (Array *)array
{
	local unsigned	i = [array count];

	do {
		--i;
		[self removeObject: [array objectAtIndex: i]];
	} while (i);
}

- (void) makeObjectsPerformSelector: (SEL)selector
{
	local int	i;

	for (i = 0; i < [self count]; i++) {
		[[self objectAtIndex: i] performSelector: selector];
	}
}

- (void) makeObjectsPerformSelector: (SEL)selector
                         withObject: (id)anObject
{
	local int	i;

	for (i = 0; i < [self count]; i++) {
		[[self objectAtIndex: i] performSelector: selector withObject: anObject];
	}
}

- (void) dealloc
{
	local unsigned	i;
	for (i = 0; i < count; i++) {
		if (_objs[i])
			[_objs[i] release];
	}

	if (_objs) {
		obj_free (_objs);
	}

	[super dealloc];
}

#if 0
/*
	This method is a sort of placeholder for when strings work.
*/
- (string) description
{
	string desc = "(";
	for (i = 0; i < [self count]; i++) {
		if (i)
			desc += ", ";
		desc += [[self objectAtIndex: i] description];
	}
	desc += ")";
	return desc;
}
#endif

@end
