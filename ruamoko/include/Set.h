#ifndef __ruamoko_Set_h
#define __ruamoko_Set_h

typedef struct set_s *set_t;
typedef struct set_iter_s *set_iter_t;

@extern void set_del_iter (set_iter_t *set_iter);
@extern unsigned set_iter_element (set_iter_t *set_iter);
@extern set_t *set_new (void);
@extern void set_delete (set_t *set);
@extern set_t *set_add (set_t *set, unsigned x);
@extern set_t *set_remove (set_t *set, unsigned x);
@extern set_t *set_invert (set_t *set);
@extern set_t *set_union (set_t *dst, set_t *src);
@extern set_t *set_intersection (set_t *dst, set_t *src);
@extern set_t *set_difference (set_t *dst, set_t *src);
@extern set_t *set_reverse_difference (set_t *dst, set_t *src);
@extern set_t *set_assign (set_t *dst, set_t *src);
@extern set_t *set_empty (set_t *set);
@extern set_t *set_everything (set_t *set);
@extern int set_is_empty (set_t *set);
@extern int set_is_everything (set_t *set);
@extern int set_is_disjoint (set_t *s1, set_t *s2);
@extern int set_is_intersecting (set_t *s1, set_t *s2);
@extern int set_is_equivalent (set_t *s1, set_t *s2);
@extern int set_is_subset (set_t *set, set_t *sub);
@extern int set_is_member (set_t *set, unsigned x);
@extern unsigned set_count (set_t *set);
@extern set_iter_t *set_first (set_t *set);
@extern set_iter_t *set_next (set_iter_t *set_iter);
@extern string set_as_string (set_t *set);

#include <Object.h>

@interface SetIterator: Object
{
	set_iter_t *iter;
}
- (SetIterator *) next;
- (unsigned) element;
@end

@interface Set: Object
{
	set_t      *set;
}
+ (id) set;
- (Set *) add: (unsigned) x;
- (Set *) remove: (unsigned) x;
- (Set *) invert;
- (Set *) union: (Set *) src;
- (Set *) intersection: (Set *) src;
- (Set *) difference: (Set *) src;
- (Set *) reverse_difference: (Set *) src;
- (Set *) assign: (Set *) other;
- (Set *) empty;
- (Set *) everything;
- (int) is_empty;
- (int) is_everything;
- (int) is_disjoint: (Set *) s2;
- (int) is_intersecting: (Set *) s2;
- (int) is_equivalent: (Set *) s2;
- (int) is_subset: (Set *) sub;
- (int) is_member: (unsigned) x;
- (int) size;
- (SetIterator *) first;
- (string) as_string;
@end

#endif//__ruamoko_Set_h
