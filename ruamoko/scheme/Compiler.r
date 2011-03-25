#include "Compiler.h"
#include "Instruction.h"
#include "Nil.h"
#include "Void.h"
#include "Boolean.h"
#include "Cons.h"
#include "defs.h"

Symbol *lambdaSym;
Symbol *quoteSym;
Symbol *defineSym;
Symbol *ifSym;
Symbol *letrecSym;
Symbol *beginSym;

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
    beginSym = symbol("begin");
    [beginSym retain];
}

+ (id) newWithLambda: (SchemeObject*) xp scope: (Scope*) sc
{
    return [[self alloc] initWithLambda: xp scope: sc];
}

- (id) initWithLambda: (SchemeObject*) xp scope: (Scope*) sc
{
    self = [super init];
    sexpr = xp;
    scope = sc;
    code = [CompiledCode new];
    err = nil;
    return self;
}

- (void) emitBuildEnvironment: (SchemeObject *) arguments
{
    local int count, index;
    local SchemeObject *cur;

    scope = [Scope newWithOuter: scope];
    count = 0;
    for (cur = arguments; [cur isKindOfClass: [Cons class]]; cur = [(Cons*) cur cdr]) {
            count++;
    }
    [code minimumArguments: count];
    if (cur != [Nil nil]) {
            count++;
    }
    [code addInstruction: [Instruction opcode: MAKEENV operand: count]];
    [code addInstruction: [Instruction opcode: LOADENV]];
    cur = arguments;
    for (index = 0; index < count; cur = [(Cons*) cur cdr]) {
            if ([cur isKindOfClass: [Cons class]]) {
                    [scope addName: (Symbol*) [(Cons*) cur car]];
                    [code addInstruction: [Instruction opcode: SET operand: index]];
            } else if ([cur isKindOfClass: [Symbol class]]) {
                    [scope addName: (Symbol*) cur];
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

- (void) emitSequence: (SchemeObject*) expressions flags: (int) fl
{
    local SchemeObject *cur;

    for (cur = expressions; cur != [Nil nil]; cur = [(Cons*) cur cdr]) {
            if ([(Cons*) cur cdr] == [Nil nil] && (fl & TAIL)) {
                    [self emitExpression: [(Cons*) cur car] flags: fl];
            } else {
                    [self emitExpression: [(Cons*) cur car] flags: fl & ~TAIL];
            }
            if (err) return;
    }
}

- (void) emitVariable: (Symbol*) sym
{
    local int depth = [scope depthOf: sym];
    local int index = [scope indexOf: sym];

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

- (void) emitDefine: (SchemeObject*) expression
{
    local int index = 0;
    
    if (![expression isKindOfClass: [Cons class]] ||
        ![[(Cons*) expression cdr] isKindOfClass: [Cons class]]) {
            err = [Error type: "syntax"
                         message: "Malformed define statement"
                         by: expression];
            return;
    }

    if ([[(Cons*) expression car] isKindOfClass: [Cons class]]) {
            index = [code addConstant: [(Cons*) [(Cons*) expression car] car]];
            [self emitLambda: cons(lambdaSym,
                                   cons([(Cons*) [(Cons*) expression car] cdr],
                                        [(Cons*) expression cdr]))];
            if (err) return;
    } else if ([[(Cons*) expression car] isKindOfClass: [Symbol class]]) {
            index = [code addConstant: [(Cons*) expression car]];
            [self emitExpression: [(Cons*) [(Cons*) expression cdr] car] flags: 0];
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

- (void) emitIf: (SchemeObject*) expression flags: (int) fl
{
    local Instruction *falseLabel, *endLabel;
    local int index;
    if (![expression isKindOfClass: [Cons class]] ||
        ![[(Cons*) expression cdr] isKindOfClass: [Cons class]]) {
            err = [Error type: "syntax"
                         message: "Malformed if expression"
                         by: expression];
    }

    falseLabel = [Instruction opcode: LABEL];
    endLabel = [Instruction opcode: LABEL];

    [self emitExpression: [(Cons*) expression car] flags: fl & ~TAIL];
    if (err) return;
    [code addInstruction: [Instruction opcode: IFFALSE label: falseLabel]];
    [self emitExpression: [(Cons*) [(Cons*) expression cdr] car] flags: fl];
    if (err) return;
    [code addInstruction: [Instruction opcode: GOTO label: endLabel]];
    [code addInstruction: falseLabel];
    if ([(Cons*) [(Cons*) expression cdr] cdr] == [Nil nil]) {
            index = [code addConstant: [Void voidConstant]];
            [code addInstruction: [Instruction opcode: LOADLITS]];
            [code addInstruction: [Instruction opcode: GET operand: index]];
    } else {
            [self emitExpression: [(Cons*) [(Cons*) [(Cons*) expression cdr] cdr] car] flags: fl];
            if (err) return;
    }
    [code addInstruction: endLabel];
}
                
            

    

- (void) emitLetrec: (SchemeObject*) expression flags: (int) fl
{
    local SchemeObject *bindings;
    local int count;

    if (!isList(expression) ||
        !isList([(Cons*) expression car]) ||
        ![[(Cons*) expression cdr] isKindOfClass: [Cons class]]) {
            err = [Error type: "syntax"
                         message: "Malformed letrec expression"
                         by: expression];
    }
    
    scope = [Scope newWithOuter: scope];
    
    count = 0;
    
    for (bindings = [(Cons*) expression car]; bindings != [Nil nil]; bindings = [(Cons*) bindings cdr]) {
            [scope addName: (Symbol*) [(Cons*) [(Cons*) bindings car] car]];
            count++;
    }

    [code addInstruction: [Instruction opcode: MAKEENV operand: count]];

    count = 0;
    
    for (bindings = [(Cons*) expression car]; bindings != [Nil nil]; bindings = [(Cons*) bindings cdr]) {
            [self emitSequence: [(Cons*) [(Cons*) bindings car] cdr] flags: fl & ~TAIL];
            [code addInstruction: [Instruction opcode: PUSH]];
            [code addInstruction: [Instruction opcode: LOADENV]];
            [code addInstruction: [Instruction opcode: SET operand: count]];
            count++;
    }

    [self emitSequence: [(Cons*) expression cdr] flags: fl];
    [code addInstruction: [Instruction opcode: POPENV]];
    scope = [scope outer];
}

- (void) emitExpression: (SchemeObject*) expression flags: (int) fl
{
    if ([expression isKindOfClass: [Cons class]]) {
            [code source: [expression source]];
            [code line: [expression line]];
            
            if ([(Cons*) expression car] == lambdaSym) {
                    [self emitLambda: expression];
            } else if ([(Cons*) expression car] == quoteSym) {
                    [self emitConstant: [(Cons*) [(Cons*) expression cdr] car]];
            } else if ([(Cons*) expression car] == defineSym) {
                    [self emitDefine: [(Cons*) expression cdr]];
            } else if ([(Cons*) expression car] == ifSym) {
                    [self emitIf: [(Cons*) expression cdr] flags: fl];
            } else if ([(Cons*) expression car] == letrecSym) {
                    [self emitLetrec: [(Cons*) expression cdr] flags: fl];
            } else if ([(Cons*) expression car] == beginSym) {
                    [self emitSequence: [(Cons*) expression cdr] flags: fl];
            } else {
                    [self emitApply: expression flags: fl];
            }
    } else if ([expression isKindOfClass: [Symbol class]]) {
            [self emitVariable: (Symbol*) expression];
    } else {
            [self emitConstant: expression];
    }
}

- (void) emitArguments: (SchemeObject*) expression
{
    if (expression == [Nil nil]) {
            return;
    } else {
            [self emitArguments: [(Cons*) expression cdr]];
            if (err) return;
            [self emitExpression: [(Cons*) expression car] flags: 0];
            if (err) return;
            [code addInstruction: [Instruction opcode: PUSH]];
    }
}

- (void) emitApply: (SchemeObject*) expression flags: (int) fl
{
    local Instruction *label = [Instruction opcode: LABEL];
    if (!(fl & TAIL)) {
            [code addInstruction: [Instruction opcode: MAKECONT label: label]];
    }
    [self emitArguments: [(Cons*) expression cdr]];
    if (err) return;
    [self emitExpression: [(Cons*) expression car] flags: fl & ~TAIL];
    if (err) return;
    [code addInstruction: [Instruction opcode: CALL]];
    [code addInstruction: label];
}

- (void) emitLambda: (SchemeObject*) expression
{
    local Compiler *compiler = [Compiler newWithLambda: expression
                                        scope: scope];
    local SchemeObject *res;
    local int index;

    res = [compiler compile];
    if ([res isError]) {
            err = (Error *) res;
            return;
    }          
    index = [code addConstant: res];
    [code addInstruction: [Instruction opcode: LOADLITS]];
    [code addInstruction: [Instruction opcode: GET operand: index]];
    [code addInstruction: [Instruction opcode: MAKECLOSURE]];
}

- (void) emitConstant: (SchemeObject*) expression
{
    local int index;
    index = [code addConstant: expression];
    [code addInstruction: [Instruction opcode: LOADLITS]];
    [code addInstruction: [Instruction opcode: GET operand: index]];
}

- (void) checkLambdaSyntax: (SchemeObject*) expression
{
    if (![expression isKindOfClass: [Cons class]] ||
        [(Cons*) expression car] != lambdaSym ||
        [(Cons*) expression cdr] == [Nil nil] ||
        [(Cons*) [(Cons*) expression cdr] cdr] == [Nil nil]) {
            err = [Error type: "syntax"
                         message: "malformed lambda expression"
                         by: expression];
    }
}

- (SchemeObject*) compile
{
    [self checkLambdaSyntax: sexpr];
    if (err) {
            return err;
    }
    [self emitBuildEnvironment: [(Cons*) [(Cons*) sexpr cdr] car]];
    if (err) {
            return err;
    }
    [self emitSequence: [(Cons*) [(Cons*) sexpr cdr] cdr] flags: TAIL];
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
