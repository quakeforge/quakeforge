#include "Set.h"

void set_del_iter (set_iter_t *set_iter) = #0;
set_t *set_new (void) = #0;
void set_delete (set_t *set) = #0;
set_t *set_add (set_t *set, unsigned x) = #0;
set_t *set_remove (set_t *set, unsigned x) = #0;
set_t *set_invert (set_t *set) = #0;
set_t *set_union (set_t *dst, set_t *src) = #0;
set_t *set_intersection (set_t *dst, set_t *src) = #0;
set_t *set_difference (set_t *dst, set_t *src) = #0;
set_t *set_reverse_difference (set_t *dst, set_t *src) = #0;
set_t *set_assign (set_t *dst, set_t *src) = #0;
set_t *set_empty (set_t *set) = #0;
set_t *set_everything (set_t *set) = #0;
int set_is_empty (set_t *set) = #0;
int set_is_everything (set_t *set) = #0;
int set_is_disjoint (set_t *s1, set_t *s2) = #0;
int set_is_intersecting (set_t *s1, set_t *s2) = #0;
int set_is_equivalent (set_t *s1, set_t *s2) = #0;
int set_is_subset (set_t *set, set_t *sub) = #0;
int set_is_member (set_t *set, unsigned x) = #0;
unsigned set_size (set_t *set) = #0;
set_iter_t *set_first (set_t *set) = #0;
set_iter_t *set_next (set_iter_t *set_iter) = #0;
string set_as_string (set_t *set) = #0;


@implementation SetIterator: Object
- initWithIterator: (set_iter_t *) iter
{
	if (!(self = [super init])) {
		return nil;
	}
	self.iter = iter;
	return self;
}

- (SetIterator *) next
{
	if ((iter = set_next (iter)))
		return self;
	[self dealloc];
	return nil;
}

- (unsigned) element = #0;
@end

@implementation Set: Object
+ (id) set
{
	return [[[Set alloc] init] autorelease];
}

- (id) init
{
	if (!(self = [super init]))
		return nil;
	set = set_new ();
	return self;
}

- (void) dealloc
{
	set_delete (set);
	[super dealloc];
}

- (Set *) add: (unsigned) x = #0;
- (Set *) remove: (unsigned) x = #0;
- (Set *) invert = #0;
- (Set *) union: (Set *) src = #0;
- (Set *) intersection: (Set *) src = #0;
- (Set *) difference: (Set *) src = #0;
- (Set *) reverse_difference: (Set *) src = #0;
- (Set *) assign: (Set *) src = #0;
- (Set *) empty = #0;
- (Set *) everything = #0;
- (int) is_empty = #0;
- (int) is_everything = #0;
- (int) is_disjoint: (Set *) s2 = #0;
- (int) is_intersecting: (Set *) s2 = #0;
- (int) is_equivalent: (Set *) s2 = #0;
- (int) is_subset: (Set *) s2 = #0;
- (int) is_member: (unsigned) x = #0;
- (int) size = #0;

- (SetIterator *) first
{
	set_iter_t *iter = set_first (set);
	SetIterator *iterator;

	if (!iter)
		return nil;
	iterator = [[SetIterator alloc] initWithIterator: iter];
	return iterator;
}

- (string) as_string = #0;
@end
