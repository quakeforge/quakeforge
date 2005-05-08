#include "Compiler.h"
#include "Instruction.h"
#include "Nil.h"
#include "Void.h"
#include "Boolean.h"
#include "Cons.h"
#include "defs.h"

Symbol lambdaSym;
Symbol quoteSym;
Symbol defineSym;
Symbol ifSym;
Symbol letrecSym;

@implementation Compiler
+ (void) initialize
{
    lambdaSym = [Symbol forString: "lambda"];
    [lambdaSym retain];
    quoteSym = [Symbol forString: "quote"];
    [quoteSym retain];
    defineSym = [Symbol forString: "define"];
    [defineSym retain];
    ifSym = [Symbol forString: "if"];
    [ifSym retain];
    letrecSym = symbol("letrec");
    [letrecSym retain];
}

+ (id) newWithLambda: (SchemeObject) xp scope: (Scope) sc
{
    return [[self alloc] initWithLambda: xp scope: sc];
}

- (id) initWithLambda: (SchemeObject) xp scope: (Scope) sc
{
    self = [super init];
    sexpr = xp;
    scope = sc;
    code = [CompiledCode new];
    err = NIL;
    return self;
}

- (void) emitBuildEnvironment: (SchemeObject) arguments
{
    local integer count, index;
    local SchemeObject cur;

    scope = [Scope newWithOuter: scope];
    count = 0;
    for (cur = arguments; [cur isKindOfClass: [Cons class]]; cur = [cur cdr]) {
            count++;
    }
    [code minimumArguments: count];
    if (cur != [Nil nil]) {
            count++;
    }
    [code addInstruction: [Instruction opcode: MAKEENV operand: count]];
    [code addInstruction: [Instruction opcode: LOADENV]];
    cur = arguments;
    for (index = 0; index < count; cur = [cur cdr]) {
            if ([cur isKindOfClass: [Cons class]]) {
                    [scope addName: (Symbol) [cur car]];
                    [code addInstruction: [Instruction opcode: SET operand: index]];
            } else if ([cur isKindOfClass: [Symbol class]]) {
                    [scope addName: (Symbol) cur];
                    [code addInstruction:
                              [Instruction opcode: SETREST operand: index]];
                    break;
            } else {
                    err = [Error type: "syntax"
                                 message: "Invalid entry in argument list"
                                 by: arguments];
                    return;
            }
            index++;
    }       
}

- (void) emitSequence: (SchemeObject) expressions flags: (integer) fl
{
    local SchemeObject cur;

    for (cur = expressions; cur != [Nil nil]; cur = [cur cdr]) {
            if ([cur cdr] == [Nil nil] && (fl & TAIL)) {
                    [self emitExpression: [cur car] flags: fl];
            } else {
                    [self emitExpression: [cur car] flags: fl & ~TAIL];
            }
            if (err) return;
    }
}

- (void) emitVariable: (Symbol) sym
{
    local integer depth = [scope depthOf: sym];
    local integer index = [scope indexOf: sym];

    [code addInstruction: [Instruction opcode: LOADENV]];
    if (depth != -1) {
            for (; depth; depth--) {
                    [code addInstruction: [Instruction opcode: GETLINK]];
            }
            [code addInstruction: [Instruction opcode: GET operand: index]];
    } else {
            index = [code addConstant: sym];
            [code addInstruction: [Instruction opcode: LOADLITS]];
            [code addInstruction: [Instruction opcode: GET operand: index]];
            [code addInstruction: [Instruction opcode: GETGLOBAL]];
    }                          
}

- (void) emitDefine: (SchemeObject) expression
{
    local integer index = 0;
    
    if (![expression isKindOfClass: [Cons class]] ||
        ![[expression cdr] isKindOfClass: [Cons class]]) {
            err = [Error type: "syntax"
                         message: "Malformed define statement"
                         by: expression];
            return;
    }

    if ([[expression car] isKindOfClass: [Cons class]]) {
            index = [code addConstant: [[expression car] car]];
            [self emitLambda: cons(lambdaSym,
                                   cons([[expression car] cdr],
                                        [expression cdr]))];
            if (err) return;
    } else if ([[expression car] isKindOfClass: [Symbol class]]) {
            index = [code addConstant: [expression car]];
            [self emitExpression: [[expression cdr] car] flags: 0];
            if (err) return;
    } else {
            err = [Error type: "syntax"
                         message: "Malformed define statement"
                         by: expression];
            return;
    }
    [code addInstruction: [Instruction opcode: PUSH]];
    [code addInstruction: [Instruction opcode: LOADLITS]];
    [code addInstruction: [Instruction opcode: GET operand: index]];
    [code addInstruction: [Instruction opcode: SETGLOBAL]];
}

- (void) emitIf: (SchemeObject) expression flags: (integer) fl
{
    local Instruction falseLabel, endLabel;
    local integer index;
    if (![expression isKindOfClass: [Cons class]] ||
        ![[expression cdr] isKindOfClass: [Cons class]]) {
            err = [Error type: "syntax"
                         message: "Malformed if expression"
                         by: expression];
    }

    falseLabel = [Instruction opcode: LABEL];
    endLabel = [Instruction opcode: LABEL];

    [self emitExpression: [expression car] flags: fl & ~TAIL];
    if (err) return;
    [code addInstruction: [Instruction opcode: IFFALSE label: falseLabel]];
    [self emitExpression: [[expression cdr] car] flags: fl];
    if (err) return;
    [code addInstruction: [Instruction opcode: GOTO label: endLabel]];
    [code addInstruction: falseLabel];
    if ([[expression cdr] cdr] == [Nil nil]) {
            index = [code addConstant: [Void voidConstant]];
            [code addInstruction: [Instruction opcode: LOADLITS]];
            [code addInstruction: [Instruction opcode: GET operand: index]];
    } else {
            [self emitExpression: [[[expression cdr] cdr] car] flags: fl];
            if (err) return;
    }
    [code addInstruction: endLabel];
}
                
            

    

- (void) emitLetrec: (SchemeObject) expression flags: (integer) fl
{
    local SchemeObject bindings;
    local integer count;

    if (!isList(expression) ||
        !isList([expression car]) ||
        ![[expression cdr] isKindOfClass: [Cons class]]) {
            err = [Error type: "syntax"
                         message: "Malformed letrec expression"
                         by: expression];
    }
    
    scope = [Scope newWithOuter: scope];
    
    count = 0;
    
    for (bindings = [expression car]; bindings != [Nil nil]; bindings = [bindings cdr]) {
            [scope addName: (Symbol) [[bindings car] car]];
            count++;
    }

    [code addInstruction: [Instruction opcode: MAKEENV operand: count]];

    count = 0;
    
    for (bindings = [expression car]; bindings != [Nil nil]; bindings = [bindings cdr]) {
            [self emitSequence: [[bindings car] cdr] flags: fl & ~TAIL];
            [code addInstruction: [Instruction opcode: PUSH]];
            [code addInstruction: [Instruction opcode: LOADENV]];
            [code addInstruction: [Instruction opcode: SET operand: count]];
            count++;
    }

    [self emitSequence: [expression cdr] flags: fl];
    [code addInstruction: [Instruction opcode: POPENV]];
    scope = [scope outer];
}

- (void) emitExpression: (SchemeObject) expression flags: (integer) fl
{
    if ([expression isKindOfClass: [Cons class]]) {
            [code source: [expression source]];
            [code line: [expression line]];
            
            if ([expression car] == lambdaSym) {
                    [self emitLambda: expression];
            } else if ([expression car] == quoteSym) {
                    [self emitConstant: [[expression cdr] car]];
            } else if ([expression car] == defineSym) {
                    [self emitDefine: [expression cdr]];
            } else if ([expression car] == ifSym) {
                    [self emitIf: [expression cdr] flags: fl];
            } else if ([expression car] == letrecSym) {
                    [self emitLetrec: [expression cdr] flags: fl];
            } else {
                    [self emitApply: expression flags: fl];
            }
    } else if ([expression isKindOfClass: [Symbol class]]) {
            [self emitVariable: (Symbol) expression];
    } else {
            [self emitConstant: expression];
    }
}

- (void) emitArguments: (SchemeObject) expression
{
    if (expression == [Nil nil]) {
            return;
    } else {
            [self emitArguments: [expression cdr]];
            if (err) return;
            [self emitExpression: [expression car] flags: 0];
            if (err) return;
            [code addInstruction: [Instruction opcode: PUSH]];
    }
}

- (void) emitApply: (SchemeObject) expression flags: (integer) fl
{
    local Instruction label = [Instruction opcode: LABEL];
    if (!(fl & TAIL)) {
            [code addInstruction: [Instruction opcode: MAKECONT label: label]];
    }
    [self emitArguments: [expression cdr]];
    if (err) return;
    [self emitExpression: [expression car] flags: fl & ~TAIL];
    if (err) return;
    [code addInstruction: [Instruction opcode: CALL]];
    [code addInstruction: label];
}

- (void) emitLambda: (SchemeObject) expression
{
    local Compiler compiler = [Compiler newWithLambda: expression
                                        scope: scope];
    local SchemeObject res;
    local integer index;

    res = [compiler compile];
    if ([res isError]) {
            err = (Error) res;
            return;
    }          
    index = [code addConstant: res];
    [code addInstruction: [Instruction opcode: LOADLITS]];
    [code addInstruction: [Instruction opcode: GET operand: index]];
    [code addInstruction: [Instruction opcode: MAKECLOSURE]];
}

- (void) emitConstant: (SchemeObject) expression
{
    local integer index;
    index = [code addConstant: expression];
    [code addInstruction: [Instruction opcode: LOADLITS]];
    [code addInstruction: [Instruction opcode: GET operand: index]];
}

- (void) checkLambdaSyntax: (SchemeObject) expression
{
    if (![expression isKindOfClass: [Cons class]] ||
        [expression car] != lambdaSym ||
        [expression cdr] == [Nil nil] ||
        [[expression cdr] cdr] == [Nil nil]) {
            err = [Error type: "syntax"
                         message: "malformed lambda expression"
                         by: expression];
    }
}

- (SchemeObject) compile
{
    [self checkLambdaSyntax: sexpr];
    if (err) {
            return err;
    }
    [self emitBuildEnvironment: [[sexpr cdr] car]];
    if (err) {
            return err;
    }
    [self emitSequence: [[sexpr cdr] cdr] flags: TAIL];
    if (err) {
            return err;
    }
    [code addInstruction: [Instruction opcode: RETURN]];
    [code compile];
    return code;
}

- (void) markReachable
{
    [code mark];
    [sexpr mark];
    [scope mark];
}

@end
