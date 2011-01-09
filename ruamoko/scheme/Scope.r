#include "Scope.h"
#include "defs.h"

@implementation Scope

+ (id) newWithOuter: (Scope []) o
{
    return [[self alloc] initWithOuter: o];
}

- (id) initWithOuter: (Scope []) o
{
    self = [super init];
    outerScope = o;
    names = [Array new];
    return self;
}

- (integer) indexLocal: (Symbol []) sym
{
    local integer index;

    for (index = 0; index < [names count]; index++) {
            if (sym == [names objectAtIndex: index]) {
                    return index;
            }
    }
    return -1;
}

- (integer) indexOf: (Symbol []) sym
{
    local integer index;

    index = [self indexLocal: sym];

    if (index < 0 && outerScope) {
            return [outerScope indexOf: sym];
    } else {
            return index;
    }
}

- (integer) depthOf: (Symbol []) sym
{
    local integer index;
    local integer res;

    index = [self indexLocal: sym];

    if (index < 0) {
            if (outerScope) {
                    res = [outerScope depthOf: sym];
                    if (res < 0) {
                            return -1;
                    } else {
                            return 1 + res;
                    }
            } else {
                    return -1;
            }
    } else {
            return 0;
    }
}

- (void) addName: (Symbol []) sym
{
    [names addObject: sym];
}

- (void) dealloc
{
    if (names) {
            [names release];
    }
    names = NIL;
    [super dealloc];
}

- (void) markReachable
{
    [names makeObjectsPerformSelector: @selector(mark)];
    [outerScope mark];
}

- (Scope []) outer
{
    return outerScope;
}

@end
