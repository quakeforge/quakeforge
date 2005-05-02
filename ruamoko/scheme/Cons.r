#include "string.h"
#include "Cons.h"
#include "Nil.h"
#include "defs.h"

Cons cons (SchemeObject car, SchemeObject cdr)
{
    return [Cons newWithCar: car cdr: cdr];
}

@implementation Cons

+ (id) newWithCar: (SchemeObject) a cdr: (SchemeObject) d
{
    return [[self alloc] initWithCar: a cdr: d];
}

- (id) initWithCar: (SchemeObject) a cdr: (SchemeObject) d
{
    car = a;
    cdr = d;

    if (!car) {
            print("Cons: WARNING: NIL car\n");
    } else if (!cdr) {
            print("cons: WARNING: NIL cdr\n");
    }

     return [super init];
}

- (SchemeObject) car
{
    return car;
}

- (void) car: (SchemeObject) a
{
    car = a;
}

- (SchemeObject) cdr
{
    return cdr;
}

- (void) cdr: (SchemeObject) d
{
    cdr = d;
}   

- (void) markReachable
{
    [car mark];
    [cdr mark];
}

- (string) printForm
{
    local string acc = "", res;
    local id cur, next = NIL;

    for (cur = self; cur; cur = next) {
            next = [cur cdr];
            acc = acc + [[cur car] printForm];
            if (next == [Nil nil]) {
                    next = NIL;
            } else  if (next && ![next isKindOfClass: [Cons class]]) {
                    acc = acc + " . " + [next printForm];
                    next = NIL;
            } else if (next) {
                    acc = acc + " ";
            }
    }

    res = str_new();
    str_copy(res, sprintf("(%s)", acc));
    return res;
}

@end
