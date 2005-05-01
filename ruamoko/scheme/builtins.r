#include "Void.h"
#include "Nil.h"
#include "Number.h"
#include "builtins.h"
#include "defs.h"
#include "string.h"
#include "Cons.h"
#include "Continuation.h"

Primitive print_p;
Primitive newline_p;
Primitive add_p;
Primitive map_p;
Primitive for_each_p;

SchemeObject bi_print (SchemeObject args, Machine m)
{
    print([[args car] printForm]);
    return [Void voidConstant];
}

SchemeObject bi_newline (SchemeObject args, Machine m)
{
    print("\n");
    return [Void voidConstant];
}

SchemeObject bi_add (SchemeObject args, Machine m)
{
    local integer sum = 0;
    local SchemeObject cur;

    for (cur = args; cur != [Nil nil]; cur = [cur cdr]) {
            sum += [(Number) [cur car] intValue];
    }

    return [Number newFromInt: sum];
}

SchemeObject bi_map (SchemeObject args, Machine m)
{
    local SchemeObject func = [args car];
    local SchemeObject list = [[args cdr] car];
    local SchemeObject output, cur, last, temp;
    local Continuation oldcont;
    
    if (list == [Nil nil]) {
            return list;
    } else {
            oldcont = [m continuation];
            [m stack: cons([list car], [Nil nil])];
            [m continuation: NIL];
            [func invokeOnMachine: m];
            output = last = cons([m run], [Nil nil]);
            for (cur = [list cdr]; cur != [Nil nil]; cur = [cur cdr]) {
                    [m stack: cons([cur car], [Nil nil])];
                    [func invokeOnMachine: m];
                    temp = cons([m run], [Nil nil]);
                    [last cdr: temp];
                    last = temp;
            }
            [m continuation: oldcont];
            return output;
    }
}
    
SchemeObject bi_for_each (SchemeObject args, Machine m)
{
    local SchemeObject func = [args car];
    local SchemeObject list = [[args cdr] car];
    local SchemeObject cur;
    local Continuation oldcont;
    
    if (list != [Nil nil]) { 
            oldcont = [m continuation];
            [m continuation: NIL];
            for (cur = list; cur != [Nil nil]; cur = [cur cdr]) {
                    [m stack: cons([cur car], [Nil nil])];
                    [func invokeOnMachine: m];
                    [m run];
            }
            [m continuation: oldcont];
    }
    return [Void voidConstant];
}

void builtin_init (void)
{
    print_p = [Primitive newFromFunc: bi_print];
    newline_p = [Primitive newFromFunc: bi_newline];
    add_p = [Primitive newFromFunc: bi_add];
    map_p = [Primitive newFromFunc: bi_map];
    for_each_p = [Primitive newFromFunc: bi_for_each];
}
