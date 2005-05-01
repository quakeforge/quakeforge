#include "Compiler.h"
#include "Instruction.h"
#include "Nil.h"
#include "Cons.h"
#include "defs.h"

@implementation Compiler
+ (id) newWithLambda: (SchemeObject) xp scope: (Scope) sc
{
    return [[self alloc] initWithLambda: xp scope: sc];
}

- (id) initWithLambda: (SchemeObject) xp scope: (Scope) sc
{
    self = [super init];
    sexpr = xp;
    scope = sc;
    lambdaSym = [Symbol newFromString: "lambda"];
    quoteSym = [Symbol newFromString: "quote"];
    code = [CompiledCode new];
    return self;
}

- (void) emitBuildEnvironment: (SchemeObject) arguments
{
    local integer count, index;
    local SchemeObject cur;

    scope = [Scope newWithOuter: scope];
    count = 0;
    for (cur = arguments; cur != [Nil nil]; cur = [cur cdr]) {
            count++;
    }
    [code addInstruction: [Instruction opcode: MAKEENV operand: count]];
    [code addInstruction: [Instruction opcode: LOADENV]];
    cur = arguments;
    for (index = 0; index < count; cur = [cur cdr]) {
            [scope addName: (Symbol) [cur car]];
            [code addInstruction: [Instruction opcode: SET operand: index]];
            index++;
    }       
}

- (void) emitSequence: (SchemeObject) expressions
{
    local SchemeObject cur;

    for (cur = expressions; cur != [Nil nil]; cur = [cur cdr]) {
            [self emitExpression: [cur car]];
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

- (void) emitExpression: (SchemeObject) expression
{
    if ([expression isKindOfClass: [Cons class]]) {
            if ([expression car] == lambdaSym) {
                    [self emitLambda: expression];
            } else if ([expression car] == quoteSym) {
                    [self emitConstant: [[expression cdr] car]];
            } else {
                    [self emitApply: expression];
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
            [self emitExpression: [expression car]];
            [code addInstruction: [Instruction opcode: PUSH]];
    }
}

- (void) emitApply: (SchemeObject) expression
{
    local Instruction label = [Instruction opcode: LABEL];
    [code addInstruction: [Instruction opcode: MAKECONT label: label]];
    [self emitArguments: [expression cdr]];
    [self emitExpression: [expression car]];
    [code addInstruction: [Instruction opcode: CALL]];
    [code addInstruction: label];
}

- (void) emitLambda: (SchemeObject) expression
{
    local Compiler compiler = [Compiler newWithLambda: expression
                                        scope: scope];
    local integer index;
    
    [compiler compile];
    index = [code addConstant: [compiler code]];
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

- (void) compile
{
    [self emitBuildEnvironment: [[sexpr cdr] car]];
    [self emitSequence: [[sexpr cdr] cdr]];
    [code addInstruction: [Instruction opcode: RETURN]];
    [code compile];
}

- (CompiledCode) code
{
    return code;
}
            
@end
