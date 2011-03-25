#include "List.h"

struct list_bucket_s {
	struct list_bucket_s *next;
	struct list_bucket_s **prev;
	id obj;
};
typedef struct list_bucket_s list_bucket_t;

@implementation List

- (id) init
{
	count = 0;
	head = nil;
	tail = &head;
	return self;
}

- (void) dealloc
{
	local list_bucket_t *e, *t = nil; //FIXME t uninitialized

	for (e = head; e; e = t) {
		t = e.next;
		[e.obj release];
		obj_free (e);
	}
	[super dealloc];
}

- (id) getItemAt: (int) index
{
	local list_bucket_t *e;
	if (index < 0 || index >= count)
		return nil;
	for (e = head; e && index; index--)
		e = e.next;
	return e.obj;
}

-(id) head
{
	if (!head)
		return nil;
	return head.obj;
}

-(id) tail
{
	local list_bucket_t *e = (list_bucket_t *) tail;
	if (!e)
		return nil;
	return e.obj;
}

-(void) addItemAtHead: (id) item
{
	local list_bucket_t *e = obj_malloc (@sizeof (list_bucket_t));
	e.obj = item;
	e.next = head;
	e.prev = &head;
	if (head)
		head.prev = &e.next;
	head = e;
	count++;
}

-(void) addItemAtTail: (id) item
{
	local list_bucket_t *e = obj_malloc (@sizeof (list_bucket_t));
	e.obj = item;
	e.next = nil;
	e.prev = tail;
	tail[0] = e;
	tail = &e.next;
	count++;
}

- (id) removeItem: (id) item
{
	local list_bucket_t *e;

	for (e = head; e; e = e.next) {
		if (e.obj == item) {
			e.prev[0] = e.next;
			if (e.next)
				e.next.prev = e.prev;
			obj_free (e);
			count--;
			return item;
		}
	}
	return nil;
}

- (id) removeItemAtHead
{
	local list_bucket_t *e;
	local id obj;

	if (!count)
		return nil;
	e = head;
	obj = e.obj;
	e.prev[0] = e.next;
	if (e.next)
		e.next.prev = e.prev;
	if (tail == &e.next)
		tail = e.prev;
	obj_free (e);
	count--;
	return obj;
}

- (id) removeItemAtTail
{
	local list_bucket_t *e;
	local id obj;

	if (!count)
		return nil;
	e = (list_bucket_t *) tail;
	obj = e.obj;
	e.prev[0] = e.next;
	if (e.next)
		e.next.prev = e.prev;
	obj_free (e);
	count--;
	return obj;
}

- (int) count
{
	return count;
}

-(void)makeObjectsPerformSelector:(SEL)selector
{
	local list_bucket_t *e;
	for (e = head; e; e = e.next)
		[e.obj performSelector:selector];
}

-(void)makeObjectsPerformSelector:(SEL)selector withObject:(id)arg
{
	local list_bucket_t *e;
	for (e = head; e; e = e.next)
		[e.obj performSelector:selector withObject:arg];
}

@end
