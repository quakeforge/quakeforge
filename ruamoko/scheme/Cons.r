#include "string.h"
#include "Cons.h"
#include "Nil.h"
#include "defs.h"
#include "SchemeString.h"

Cons []cons (SchemeObject []car, SchemeObject []cdr)
{
    return [Cons newWithCar: car cdr: cdr];
}

integer length (SchemeObject []foo)
{
    local integer len;

    for (len = 0; [foo isKindOfClass: [Cons class]]; foo = [(Cons []) foo cdr]) {
            len++;
    }

    return len;
}

BOOL isList (SchemeObject []ls)
{
    return ls == [Nil nil] ||
        ([ls isKindOfClass: [Cons class]] &&
         isList([(Cons[]) ls cdr]));
}

@implementation Cons

+ (id) newWithCar: (SchemeObject []) a cdr: (SchemeObject []) d
{
    return [[self alloc] initWithCar: a cdr: d];
}

- (id) initWithCar: (SchemeObject []) a cdr: (SchemeObject []) d
{
    car = a;
    cdr = d;

    if (!car) {
            print("Cons: WARNING: nil car\n");
    } else if (!cdr) {
            print("cons: WARNING: nil cdr\n");
    }

     return [super init];
}

- (SchemeObject []) car
{
    return car;
}

- (void) car: (SchemeObject []) a
{
    car = a;
}

- (SchemeObject []) cdr
{
    return cdr;
}

- (void) cdr: (SchemeObject []) d
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
    local string acc = "";
    local id cur, next = nil;

    for (cur = self; cur; cur = next) {
            next = [cur cdr];
            acc = acc + [[cur car] printForm];
            if (next == [Nil nil]) {
                    next = nil;
            } else  if (next && ![next isKindOfClass: [Cons class]]) {
                    acc = acc + " . " + [next printForm];
                    next = nil;
            } else if (next) {
                    acc = acc + " ";
            }
    }

    return [[String newFromString: sprintf("(%s)", acc)] stringValue];
}

@end
