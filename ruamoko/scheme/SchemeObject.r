#include "SchemeObject.h"
#include "defs.h"

SchemeObject maybe_garbage, not_garbage, roots;
BOOL markstate;

BOOL contains (SchemeObject list, SchemeObject what)
{
    local SchemeObject cur;

    for (cur = list; cur; cur = cur.next) {
            if (cur == what)
                    return true;
    }

    return false;
}



@implementation SchemeObject

+ (void) initialize
{
    maybe_garbage = not_garbage = roots = NIL;
    markstate = true;
}

+ (id) dummyObject
{
    return [[SchemeObject alloc] initDummy];
}

- (id) initDummy
{
    self = [super init];
    prev = next = NIL;
    marked = markstate;
    root = false;
    return self;
}

+ (void) collect
{
    local SchemeObject cur, next = NIL, dummy;

    not_garbage = dummy = [SchemeObject dummyObject];
    for (cur = roots; cur; cur = next) {
            next = cur.next;
            [cur markReachable];
    }
    for (cur = dummy; cur; cur = cur.prev) {
            dprintf("GC: marking queue: %s[%s]@%i\n", [cur description], [cur printForm],
                    (integer) cur);
            [cur markReachable];
    }
    for (cur = maybe_garbage; cur; cur = next) {
            next = cur.next;
            dprintf("GC: freeing %s[%s]@%i...\n", [cur description], [cur printForm], (integer) cur);
            [cur release];
    }
    maybe_garbage = not_garbage;
    not_garbage = NIL;
    markstate = !markstate;
}

- (id) init
{
    self = [super init];
    if (maybe_garbage) {
            maybe_garbage.prev = self;
    }
    next = maybe_garbage;
    maybe_garbage = self;
    prev = NIL;
    marked = !markstate;
    root = false;
    return self;
}

- (void) mark
{
    if (!root && marked != markstate) {
            dprintf("GC: Marking %s[%s]@%i\n", [self description], [self printForm], (integer) self);
            marked = markstate;
            if (prev) {
                    prev.next = next;
            } else {
                    maybe_garbage = next;
                    }
            if (next) {
                    next.prev = prev;
            }
            if (not_garbage) {
                    not_garbage.prev = self;
            }
            next = not_garbage;
            prev = NIL;
            not_garbage = self;
                //[self markReachable];
    }
}

- (void) makeRootCell
{
    if (prev) {
            prev.next = next;
    } else {
            maybe_garbage = next;
    }
    if (next) {
            next.prev = prev;
    }
    if (roots) {
            roots.prev = self;
    }
    next = roots;
    prev = NIL;
    roots = self;
    root = true;
}

- (void) markReachable
{
    return;
}

- (string) printForm
{
    return "<generic>";
}

- (void) dealloc
{
    dprintf("Deallocing %s @ %i!\n", [self description], (integer) self);
    [super dealloc];
}

- (BOOL) isError
{
    return false;
}

- (string) source
{
    return source;
}

- (void) source: (string) s
{
    source = s;
}

- (integer) line
{
    return line;
}

- (void) line: (integer) l
{
    line = l;
}

@end
