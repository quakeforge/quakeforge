#include "Lambda.h"
#include "Nil.h"
#include "Symbol.h"
#include "string.h"
#include "defs.h"

/*
SchemeObject evaluate (SchemeObject expr, SchemeObject env);

SchemeObject extend_environment (SchemeObject baseenv, SchemeObject argnames, SchemeObject argvalues)
{
    local SchemeObject name, value;

    if (argnames == [Nil nil]) {
            return baseenv;
    }

    name = [argnames car];
    value = [argvalues car];
    return [Cons newWithCar: [Cons newWithCar: name cdr: value] cdr:
                     extend_environment([baseenv cdr], [argnames cdr], [argvalues cdr])];
}

SchemeObject assoc (Symbol name, SchemeObject list)
{
    if (list == [Nil nil]) {
            return NIL;
    }

    printf("assoc: Comparing %s to %s\n",
           [name printForm], [[[list car] car] printForm]);
    
    if ([[list car] car] == name) {
            print("assoc: Comparison successful, returning" +
                  [[[list car] cdr] printForm] + "\n");
            return [[list car] cdr];
    } else {
            return assoc (name, [list cdr]);
    }
}

SchemeObject evaluate_list (SchemeObject list, SchemeObject env)
{
    if (list == [Nil nil]) {
            return list;
    } else {
            return [Cons newWithCar: evaluate([list car], env) cdr:
                             evaluate_list ([list cdr], env)];
    }
}

SchemeObject evaluate (SchemeObject expr, SchemeObject env)
{
    local SchemeObject res;

    print ("Entering evaluated...\n");
    
    if ([expr isKindOfClass: [Cons class]]) {
            res = evaluate_list (expr, env);
            print("Got evaluated list: " + [res printForm] + "\n");
            return [[res car] invokeWithArgs: [res cdr]];
    } else if ([expr isKindOfClass: [Symbol class]]) {
            print("Looking up symbol: " + [expr printForm] + "\n");
            return assoc((Symbol) expr, env);
    } else {
            return expr;
    }
}

*/

@implementation Lambda
+ (id) newWithCode: (CompiledCode) c environment: (Frame) e
{
    return [[self alloc] initWithCode: c environment: e];
}
            
- (id) initWithCode: (CompiledCode) c environment: (Frame) e
{
    self = [super init];
    code = c;
    env = e;
    return self;
}

- (void) invokeOnMachine: (Machine) m
{
    [m loadCode: code];
    [m environment: env];
}
    
@end
