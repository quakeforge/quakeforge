#include "Void.h"
#include "Nil.h"
#include "Number.h"
#include "builtins.h"
#include "defs.h"
#include "string.h"
#include "Cons.h"
#include "Continuation.h"
#include "Boolean.h"

SchemeObject bi_display (SchemeObject args, Machine m)
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

SchemeObject bi_cons (SchemeObject args, Machine m)
{
    [args cdr: [[args cdr] car]];
    return args;
}

SchemeObject bi_null (SchemeObject args, Machine m)
{
    return [args car] == [Nil nil]
        ?
        [Boolean trueConstant] :
        [Boolean falseConstant];
}

SchemeObject bi_car (SchemeObject args, Machine m)
{
    return [[args car] car];
}

SchemeObject bi_cdr (SchemeObject args, Machine m)
{
    return [[args car] cdr];
}

SchemeObject bi_apply (SchemeObject args, Machine m)
{
    [m stack: [[args cdr] car]];
    [[args car] invokeOnMachine: m];
    return NIL;
}

void builtin_addtomachine (Machine m)
{
    [m addGlobal: symbol("display")
       value: [Primitive newFromFunc: bi_display]];
    [m addGlobal: symbol("newline")
       value: [Primitive newFromFunc: bi_newline]];
    [m addGlobal: symbol("+")
       value: [Primitive newFromFunc: bi_add]];
    [m addGlobal: symbol("cons")
       value: [Primitive newFromFunc: bi_cons]];
    [m addGlobal: symbol("null?")
       value: [Primitive newFromFunc: bi_null]];
    [m addGlobal: symbol("car")
       value: [Primitive newFromFunc: bi_car]];
    [m addGlobal: symbol("cdr")
       value: [Primitive newFromFunc: bi_cdr]];
    [m addGlobal: symbol("apply")
       value: [Primitive newFromFunc: bi_apply]];
}
