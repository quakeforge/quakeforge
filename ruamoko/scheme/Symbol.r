#include "Symbol.h"
#include "hash.h"
#include "defs.h"

string SymbolGetKey (void [] ele, void [] data)
{
    local Symbol s = (Symbol) ele;

    return [s stringValue];
}

void SymbolFree (void [] ele, void [] data)
{
    local Symbol s = (Symbol) ele;

    [s release];
}
    
hashtab_t symbols;
Symbol lparen;
Symbol rparen;
Symbol quote;
Symbol dot;

Symbol symbol (string str)
{
    return [Symbol forString: str];
}

@implementation Symbol
+ (void) initialize
{
    symbols = Hash_NewTable (1024, SymbolGetKey, SymbolFree, NIL);
    lparen = [Symbol forString: "("];
    rparen = [Symbol forString: ")"];
    quote = [Symbol forString: "'"];
    dot = symbol(".");
    [lparen makeRootCell];
    [rparen makeRootCell];
    [quote makeRootCell];
    [dot makeRootCell];
}

+ (Symbol) forString: (string) s
{
    local Symbol res;

    if ((res = Hash_Find (symbols, s))) {
            return res;
    } else {
            res = (Symbol) [self newFromString: s];
            Hash_Add (symbols, res);
            return res;
    }
}

+ (Symbol) leftParen
{
    return lparen;
}

+ (Symbol) rightParen
{
    return rparen;
}

+ (Symbol) quote
{
    return quote;
}

+ (Symbol) dot
{
    return dot;
}

- (string) printForm
{
    return value;
}

- (void) dealloc
{
    if (Hash_Find (symbols, value) == self) {
            Hash_Del (symbols, value);
    }
    [super dealloc];
}

@end
