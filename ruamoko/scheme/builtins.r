#include "Void.h"
#include "Nil.h"
#include "Number.h"
#include "builtins.h"
#include "defs.h"
#include "string.h"
#include "Cons.h"
#include "Continuation.h"
#include "BaseContinuation.h"
#include "Boolean.h"
#include "Error.h"

BOOL num_args (SchemeObject list, integer num)
{
    for (; [list isKindOfClass: [Cons class]]; list = [list cdr]) {
            num--;
    }
    return num == 0;
}

SchemeObject bi_display (SchemeObject args, Machine m)
{
    if (!num_args(args, 1)) {
            return [Error type: "display"
                          message: "expected 1 argument"
                          by: m];
    }
    print([[args car] printForm]);
    return [Void voidConstant];
}

SchemeObject bi_newline (SchemeObject args, Machine m)
{
    if (!num_args(args, 0)) {
            return [Error type: "newline"
                          message: "expected no arguments"
                          by: m];
    }
    print("\n");
    return [Void voidConstant];
}

SchemeObject bi_add (SchemeObject args, Machine m)
{
    local integer sum = 0;
    local SchemeObject cur;

    for (cur = args; cur != [Nil nil]; cur = [cur cdr]) {
            if (![[cur car] isKindOfClass: [Number class]]) {
                    return [Error type: "+"
                                  message: sprintf("non-number argument: %s\n",
                                                   [[cur car] printForm])
                                  by: m];
            }                     
            sum += [(Number) [cur car] intValue];
    }

    return [Number newFromInt: sum];
}

SchemeObject bi_cons (SchemeObject args, Machine m)
{
    if (!num_args(args, 2)) {
            return [Error type: "cons"
                          message: "expected 2 arguments"
                          by: m];
    }
    [args cdr: [[args cdr] car]];
    return args;
}

SchemeObject bi_null (SchemeObject args, Machine m)
{
    if (!num_args(args, 1)) {
            return [Error type: "null?"
                          message: "expected 1 argument"
                          by: m];
    }
    return [args car] == [Nil nil]
        ?
        [Boolean trueConstant] :
        [Boolean falseConstant];
}

SchemeObject bi_car (SchemeObject args, Machine m)
{
    if (!num_args(args, 1)) {
            return [Error type: "car"
                          message: "expected 1 argument"
                          by: m];
    }
    if (![[args car] isKindOfClass: [Cons class]]) {
            return [Error type: "car"
                          message: sprintf("expected pair, got: %s",
                                           [[args car] printForm])
                          by: m];
    }
                
    return [[args car] car];
}

SchemeObject bi_cdr (SchemeObject args, Machine m)
{
    if (!num_args(args, 1)) {
            return [Error type: "cdr"
                          message: "expected 1 argument"
                          by: m];
    }
    if (![[args car] isKindOfClass: [Cons class]]) {
            return [Error type: "cdr"
                          message: sprintf("expected pair, got: %s",
                                           [[args car] printForm])
                          by: m];
    }
    return [[args car] cdr];
}

SchemeObject bi_apply (SchemeObject args, Machine m)
{
    if (args == [Nil nil]) {
            return [Error type: "apply"
                          message: "expected at least 1 argument"
                          by: m];
    } else if (![[args car] isKindOfClass: [Procedure class]]) {
            return [Error type: "apply"
                          message:
                              sprintf("expected procedure as 1st argument, got: %s",
                                      [[args car] printForm])
                          by: m];
    }
    
    [m stack: [[args cdr] car]];
    [[args car] invokeOnMachine: m];
    return NIL;
}

SchemeObject bi_callcc (SchemeObject args, Machine m)
{
    if (args == [Nil nil]) {
            return [Error type: "call-with-current-continuation"
                          message: "expected at least 1 argument"
                          by: m];
    } else if (![[args car] isKindOfClass: [Procedure class]]) {
            return [Error type: "call-with-current-continuation"
                          message:
                              sprintf("expected procedure as 1st argument, got: %s",
                                      [[args car] printForm])
                          by: m];
    }
    if ([m continuation]) {
            [m stack: cons([m continuation], [Nil nil])];
    } else {
            [m stack: cons([BaseContinuation baseContinuation],
                           [Nil nil])];
    }
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
    [m addGlobal: symbol("call-with-current-continuation")
       value: [Primitive newFromFunc: bi_callcc]];
}
