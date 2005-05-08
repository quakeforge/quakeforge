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

SchemeObject bi_sub (SchemeObject args, Machine m)
{
    local integer diff = 0;
    local SchemeObject cur;

    if (args == [Nil nil]) {
            return [Error type: "-"
                          message: sprintf("expected at least 1 argument")
                          by: m];
    }

    cur = [args car];
    
    if (![cur isKindOfClass: [Number class]]) {
            return [Error type: "-"
                          message: sprintf("non-number argument: %s\n",
                                           [cur printForm])
                          by: m];
    }

    diff = [(Number) cur intValue];
    
    if ([args cdr] == [Nil nil]) {
            return [Number newFromInt: -diff];
    }
    
    for (cur = [args cdr]; cur != [Nil nil]; cur = [cur cdr]) {
            if (![[cur car] isKindOfClass: [Number class]]) {
                    return [Error type: "-"
                                  message: sprintf("non-number argument: %s\n",
                                                   [[cur car] printForm])
                                  by: m];
            }                     
            diff -= [(Number) [cur car] intValue];
    }

    return [Number newFromInt: diff];
}

SchemeObject bi_mult (SchemeObject args, Machine m)
{
    local integer prod = 1;
    local SchemeObject cur;

    for (cur = args; cur != [Nil nil]; cur = [cur cdr]) {
            if (![[cur car] isKindOfClass: [Number class]]) {
                    return [Error type: "*"
                                  message: sprintf("non-number argument: %s\n",
                                                   [[cur car] printForm])
                                  by: m];
            }                     
            prod *= [(Number) [cur car] intValue];
    }

    return [Number newFromInt: prod];
}

SchemeObject bi_div (SchemeObject args, Machine m)
{
    local integer frac = 0;
    local SchemeObject cur;

    if (args == [Nil nil]) {
            return [Error type: "/"
                          message: sprintf("expected at least 1 argument")
                          by: m];
    }

    cur = [args car];
    
    if (![cur isKindOfClass: [Number class]]) {
            return [Error type: "/"
                          message: sprintf("non-number argument: %s\n",
                                           [cur printForm])
                          by: m];
    }

    frac = [(Number) cur intValue];
    
    if ([args cdr] == [Nil nil]) {
            return [Number newFromInt: 1/frac];
    }
    
    for (cur = [args cdr]; cur != [Nil nil]; cur = [cur cdr]) {
            if (![[cur car] isKindOfClass: [Number class]]) {
                    return [Error type: "/"
                                  message: sprintf("non-number argument: %s\n",
                                                   [[cur car] printForm])
                                  by: m];
            }                     
            frac /= [(Number) [cur car] intValue];
    }

    return [Number newFromInt: frac];
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

SchemeObject bi_eq (SchemeObject args, Machine m)
{
    if (!num_args(args, 2)) {
            return [Error type: "eq?"
                          message: "expected 2 arguments"
                          by: m];
    }
    return
        [args car] == [[args cdr] car] ?
        [Boolean trueConstant] :
        [Boolean falseConstant];
}

SchemeObject bi_numeq (SchemeObject args, Machine m)
{
    local SchemeObject num1, num2;
    if (!num_args(args, 2)) {
            return [Error type: "="
                          message: "expected 2 arguments"
                          by: m];
    }
    num1 = [args car];
    num2 = [[args cdr] car];
    if (![num1 isKindOfClass: [Number class]]) {
            return [Error type: "="
                          message: sprintf("expected number argument, got: %s",
                                           [num1 printform])
                          by: m];
    } else if (![num2 isKindOfClass: [Number class]]) {
            return [Error type: "="
                          message: sprintf("expected number argument, got: %s",
                                           [num2 printform])
                          by: m];
    }

    return
        [num1 intValue] == [num2 intValue] ?
        [Boolean trueConstant] :
        [Boolean falseConstant];
}

SchemeObject bi_islist (SchemeObject args, Machine m)
{
    if (!num_args(args, 1)) {
            return [Error type: "list?"
                          message: "expected 1 argument"
                          by: m];
    }

    return
        isList (args) ?
        [Boolean trueConstant] :
        [Boolean falseConstant];
}

SchemeObject bi_ispair (SchemeObject args, Machine m)
{
    if (!num_args(args, 1)) {
            return [Error type: "pair?"
                          message: "expected 1 argument"
                          by: m];
    }

    return
        [[args car] isKindOfClass: [Cons class]] ?
        [Boolean trueConstant] :
        [Boolean falseConstant];
}

#define builtin(name, func) [m addGlobal: symbol(#name) value: [Primitive newFromFunc: (func)]]

void builtin_addtomachine (Machine m)
{
    builtin(display, bi_display);
    builtin(newline, bi_newline);
    builtin(+, bi_add);
    builtin(-, bi_sub);
    builtin(*, bi_mult);
    builtin(/, bi_div);
    builtin(cons, bi_cons);
    builtin(null?, bi_null);
    builtin(car, bi_car);
    builtin(cdr, bi_cdr);
    builtin(apply, bi_apply);
    builtin(call-with-current-continuation, bi_callcc);
    builtin(eq?, bi_eq);
    builtin(=, bi_numeq);
    builtin(list?, bi_islist);
    builtin(pair?, bi_ispair);
}
