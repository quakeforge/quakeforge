#include "SchemeObject.h"
#include "defs.h"
//#include "debug.h"

SchemeObject *maybe_garbage, *not_garbage, *not_garbage_end, *wait_list, *roots, *queue_pos;
BOOL markstate;

typedef enum {
    GC_IDLE = 0,
    GC_MARK = 1,
    GC_SWEEP = 2
} gc_state_e;

gc_state_e gc_state;
int checkpoint;

#define GC_AMOUNT 100

@implementation SchemeObject

+ (void) initialize
{
    maybe_garbage = not_garbage = not_garbage_end = wait_list = roots = nil;
    markstate = true;
    gc_state = GC_IDLE;
    checkpoint = 0;
}

- (id) initDummy
{
    self = [super init];
    prev = next = nil;
    marked = markstate;
    root = false;
    return self;
}

+ (id) dummyObject
{
    return [[SchemeObject alloc] initDummy];
}

+ (void) collect
{
    local SchemeObject *cur;
    local int amount;

    switch (gc_state) {
        case GC_IDLE:
            dprintf("GC: Starting collection...\n");
            gc_state = GC_MARK;
            not_garbage = not_garbage_end = [SchemeObject dummyObject];
            for (cur = roots; cur; cur = cur.next) {
                    [cur markReachable];
            }
            queue_pos = not_garbage_end;
            return;
        case GC_MARK:
            dprintf("GC: Marking...\n");
            amount = 0;
            while (queue_pos) {
                    dprintf("GC: marking queue: %s[%s]@%i\n",
                            [queue_pos description],
                            [queue_pos printForm],
                            (int) queue_pos);
                    [queue_pos markReachable];
                    queue_pos = queue_pos.prev;
                    if (++amount >= GC_AMOUNT/2) return;
            }
            dprintf("MARKED: %i reachable objects\n", amount);
            gc_state = GC_SWEEP;
            queue_pos = maybe_garbage;
            return;
        case GC_SWEEP:
            dprintf("GC: Sweeping...\n");
            amount = 0;
            while (queue_pos) {
                    dprintf("GC: freeing %s[%s]@%i...\n",
                            [queue_pos description],
                            [queue_pos printForm],
                            (int) queue_pos);
                    [queue_pos release];
                    queue_pos = queue_pos.next;
                        //if (++amount == GC_AMOUNT) return;
                    }
            maybe_garbage = not_garbage;
            not_garbage_end.next = wait_list;
            if (wait_list) {
                    wait_list.prev = not_garbage_end;
            }
            wait_list = nil;
            not_garbage_end = nil;
            not_garbage = nil;
            markstate = !markstate;
            gc_state = GC_IDLE;
    }
}

+ (void) collectCheckPoint
{
    if (checkpoint >= GC_AMOUNT)
    {
            [self collect];
            checkpoint = 0;
    }
}

+ (void) finishCollecting
{
    while (gc_state) {
            [self collect];
    }
}

- (id) init
{
    self = [super init];

    if (gc_state) {
            if (wait_list) {
                    wait_list.prev = self;
            }
            next = wait_list;
            wait_list = self;
            marked = markstate;
            dprintf("GC: During collect: %i\n", (int) self);
    } else {
            if (maybe_garbage) {
                    maybe_garbage.prev = self;
            }
            next = maybe_garbage;
            maybe_garbage = self;
            marked = !markstate;
            dprintf("GC: Not during collect: %i\n", (int) self);
    }
    
    prev = nil;
    root = false;
    checkpoint++;
    return self;
}

- (void) mark
{
    if (!root && marked != markstate) {
            dprintf("GC: Marking %s[%s]@%i\n", [self description], [self printForm], (int) self);
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
            prev = nil;
            not_garbage = self;
    }
}

- (void) makeRootCell
{
    if (root)
            return;
    if (gc_state) {
            dprintf("Root cell made during collection!\n");
            [SchemeObject finishCollecting];
    }
            
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
    prev = nil;
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
    dprintf("Deallocing %s @ %i!\n", [self description], (int) self);
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

- (int) line
{
    return line;
}

- (void) line: (int) l
{
    line = l;
}

@end
