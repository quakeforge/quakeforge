#include "Machine.h"
#include "Cons.h"
#include "Lambda.h"
#include "Boolean.h"
#include "Nil.h"
#include "defs.h"
#include "string.h"
#include "Error.h"
//#include "debug.h"

string GlobalGetKey (void *ele, void *data)
{
    return [[((Cons*) ele) car] printForm];
}

void GlobalFree (void *ele, void *data)
{
    return;
}

@implementation Machine

- (id) init
{
    self = [super init];
    state.program = nil;
    state.pc = 0;
    value = nil;
    state.cont = nil;
    state.env = nil;
    state.literals = nil;
    state.proc = nil;
    state.stack = [Nil nil];
    state.lineinfo = nil;
    globals = Hash_NewTable(1024, GlobalGetKey, GlobalFree, nil);
    all_globals = [Nil nil];
    return self;
}

- (void) addGlobal: (Symbol*) sym value: (SchemeObject*) val
{
    local Cons *c = cons(sym, val);
    Hash_Add(globals, c);
    all_globals = cons(c, all_globals);
}

- (void) loadCode: (CompiledCode*) code
{
    state.program = [code code];
    state.literals = [code literals];
    state.lineinfo = [code lineinfo];
    state.pc = 0;
}

- (void) environment: (Frame*) e
{
    state.env = e;
}

- (void) continuation: (Continuation*) c
{
    state.cont = c;
}

- (void) value: (SchemeObject*) v
{
    value = v;
}

- (SchemeObject*) value
{
    return value;
}

- (Continuation*) continuation
{
    return state.cont;
}

- (SchemeObject*) stack
{
    return state.stack;
}

- (void) stack: (SchemeObject*) o
{
    state.stack = o;
}

- (state_t *) state
{
    return &state;
}

- (void) state: (state_t *) st
{
    state.program = st.program;
    state.pc = st.pc;
    state.literals = st.literals;
    state.stack = st.stack;
    state.cont = st.cont;
    state.env = st.env;
    state.proc = st.proc;
    state.lineinfo = st.lineinfo;
}

- (void) procedure: (Procedure*) pr
{
    state.proc = pr;
}

- (SchemeObject*) run
{
    local integer opcode;
    local integer operand;
    local SchemeObject *res;
    while (1) {
            if (value && [value isError]) {
                    dprintf("Error: %s[%s]\n", [value description], [value printForm]);
                    return value;
            }
            opcode = state.program[state.pc].opcode;
            operand = state.program[state.pc].operand;
            state.pc = state.pc + 1;

            
            switch (opcode) {
                case PUSH:
                    if (value) {
                            dprintf("Push: %s\n", [value printForm]);
                    } else {
                            dprintf("Push: NULL!!!!\n");
                    }
                    state.stack = cons(value, state.stack);
                    break;
                case POP:
                    value = [(Cons*) state.stack car];
                    if (value) {
                            dprintf("Pop: %s\n", [value printForm]);
                    } else {
                            dprintf("Pop: NULL!!!!\n");
                    }
                    state.stack = [(Cons*) state.stack cdr];
                    break;
                case MAKECLOSURE:
                    dprintf("Makeclosure\n");
                    value = [Lambda newWithCode: (CompiledCode*) value
                                    environment: state.env];
                    break;
                case MAKECONT:
                    dprintf("Makecont\n");
                    state.cont = [Continuation newWithState: &state
                                               pc: operand];
                    state.stack = [Nil nil];
                    break;
                case LOADENV:
                    dprintf("Loadenv\n");
                    value = state.env;
                    break;
                case LOADLITS:
                    dprintf("Loadlits\n");
                    value = state.literals;
                    break;
                case MAKEENV:
                    dprintf("Makeenv\n");
                    state.env = [Frame newWithSize: operand link: state.env];
                    break;
                case POPENV:
                    dprintf("Popenv\n");
                    state.env = [state.env getLink];
                case GET:
                    value = [(Frame*) value get: operand];
                    dprintf("Get: %i --> %s\n", operand, [value printForm]);
                    break;
                case SET:
                    [(Frame*) value set: operand to: [(Cons*) state.stack car]];
                    dprintf("Set: %i --> %s\n", operand, [[(Cons*) state.stack car] printForm]);
                    state.stack = [(Cons*) state.stack cdr];
                    break;
                case SETREST:
                    [(Frame*) value set: operand to: state.stack];
                    dprintf("Setrest: %i --> %s\n", operand, [state.stack printForm]);
                    state.stack = [Nil nil];
                    break;
                case SETSTACK:
                    dprintf("Setstack: %s\n", [value printForm]);
                    state.stack = value;
                    break;
                case GETLINK:
                    dprintf("Getlink\n");
                    value = [(Frame*) value getLink];
                    break;
                case GETGLOBAL:
                    dprintf("Getglobal: %s\n", [value printForm]);
                    res = [((Cons*) Hash_Find(globals, [value printForm])) cdr];
                    if (!res) {
                            return [Error type: "binding"
                                          message: sprintf("Undefined binding: %s",
                                                           [value printForm])
                                          by: self];
                    }
                    value = res;
                    dprintf(" --> %s\n", [value printForm]);
                    break;
                case SETGLOBAL:
                    dprintf("Setglobal: %s\n", [value printForm]);
                    [self addGlobal: (Symbol*) value value: [(Cons*) state.stack car]];
                    state.stack = [(Cons*) state.stack cdr];
                    break;
                case CALL:
                    dprintf("Call\n");
                    [SchemeObject collectCheckPoint];
                    if (![value isKindOfClass: [Procedure class]]) {
                            return [Error type: "call"
                                          message:
                                              sprintf("Attempted to apply non-procedure: %s. Arguments were: %s",
                                                      [value printForm], [state.stack printForm])
                                          by: self];
                    }
                    [(Procedure*) value invokeOnMachine: self];
                    break;
                case RETURN:
                    dprintf("Return: %s\n", [value printForm]);
                    if (!state.cont) {
                            return value;
                    } else {
                            [state.cont restoreOnMachine: self];
                    }
                    break;
                case IFFALSE:
                    dprintf("Iffalse: %s\n", [value printForm]);
                    if (value == [Boolean falseConstant]) {
                            state.pc = operand;
                    }
                    break;
                case GOTO:
                    dprintf("Goto: %i\n", operand);
                    state.pc = operand;
                    break;
            }
    }
}

- (string) source
{
    if (state.lineinfo) {
            return [state.lineinfo[state.pc-1].sourcefile stringValue];
    } else {
            return [super source];
    }
}

- (integer) line
{
    if (state.lineinfo) {
            return state.lineinfo[state.pc-1].linenumber;
    } else {
            return [super line];
    }
}

- (void) markReachable
{
    [state.literals mark];
    [state.stack mark];
    [state.cont mark];
    [state.env mark];
    [state.proc mark];
    [value mark];
    [all_globals mark];
}

- (void) reset
{
    state.stack = [Nil nil];
}
@end

