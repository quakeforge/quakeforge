#include "Machine.h"
#include "Cons.h"
#include "Lambda.h"
#include "Nil.h"
#include "defs.h"

string GlobalGetKey (void []ele, void []data)
{
    return [[((Cons) ele) car] printForm];
}

void GlobalFree (void []ele, void []data)
{
    return;
}

@implementation Machine

- (id) init
{
    self = [super init];
    state.program = NIL;
    state.pc = 0;
    value = NIL;
    cont = NIL;
    env = NIL;
    state.literals = NIL;
    state.stack = [Nil nil];
    globals = Hash_NewTable(1024, GlobalGetKey, GlobalFree, NIL);
    return self;
}

- (void) addGlobal: (Symbol) sym value: (SchemeObject) val
{
    local Cons c = cons(sym, val);
    [c makeRootCell];
    Hash_Add(globals, c);
}

- (void) loadCode: (CompiledCode) code
{
    state.program = [code code];
    state.literals = [code literals];
    state.pc = 0;
}

- (void) environment: (Frame) e
{
    env = e;
}

- (void) continuation: (Continuation) c
{
    cont = c;
}

- (void) value: (SchemeObject) v
{
    value = v;
}

- (Continuation) continuation
{
    return cont;
}

- (SchemeObject) stack
{
    return state.stack;
}

- (void) stack: (SchemeObject) o
{
    state.stack = o;
}

- (state_t []) state
{
    return &state;
}

- (void) state: (state_t []) st
{
    state.program = st[0].program;
    state.pc = st[0].pc;
    state.literals = st[0].literals;
    state.stack = st[0].stack;
}

- (SchemeObject) run
{
    local integer opcode;
    local integer operand;
    while (1) {
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
                    value = [state.stack car];
                    if (value) {
                            dprintf("Pop: %s\n", [value printForm]);
                    } else {
                            dprintf("Pop: NULL!!!!\n");
                    }
                    state.stack = [state.stack cdr];
                    break;
                case MAKECLOSURE:
                    dprintf("Makeclosure\n");
                    value = [Lambda newWithCode: (CompiledCode) value
                                    environment: env];
                    break;
                case MAKECONT:
                    dprintf("Makecont\n");
                    cont = [Continuation newWithState: &state
                                         environment: env
                                         continuation: cont
                                         pc: operand];
                    break;
                case LOADENV:
                    dprintf("Loadenv\n");
                    value = env;
                    break;
                case LOADLITS:
                    dprintf("Loadlits\n");
                    value = state.literals;
                    break;
                case MAKEENV:
                    dprintf("Makeenv\n");
                    env = [Frame newWithSize: operand link: env];
                    break;
                case GET:
                    value = [value get: operand];
                    dprintf("Get: %i --> %s\n", operand, [value printForm]);
                    break;
                case SET:
                    [value set: operand to: [state.stack car]];
                    dprintf("Set: %i --> %s\n", operand, [value printForm]);
                    state.stack = [state.stack cdr];
                    break;
                case GETLINK:
                    dprintf("Getlink");
                    value = [value getLink];
                    break;
                case GETGLOBAL:
                    dprintf("Getglobal: %s\n", [value printForm]);
                    value = [((Cons) Hash_Find(globals, [value printForm])) cdr];
                    break;
                case CALL:
                    dprintf("Call\n");
                    [value invokeOnMachine: self];
                    break;
                case RETURN:
                    dprintf("Return: %s\n", [value printForm]);
                    if (!cont) {
                            return value;
                    } else {
                            [cont invokeOnMachine: self];
                    }
                    break;
            }
    }
}

- (void) markReachable
{
    [state.literals mark];
    [state.stack mark];
    [cont mark];
    [env mark];
    [value mark];
        // FIXME: need to mark globals
}
@end
