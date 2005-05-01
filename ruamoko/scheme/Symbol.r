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

@implementation Symbol
+ (void) initialize
{
    symbols = Hash_NewTable (1024, SymbolGetKey, SymbolFree, NIL);
    lparen = [Symbol forString: "("];
    rparen = [Symbol forString: ")"];
    quote = [Symbol forString: "'"];
    
}

+ (Symbol) forString: (string) s
{
    return (Symbol) [self newFromString: s];
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

- (id) initWithString: (string) s
{
    local Symbol res;
    [super initWithString: s];

    if ((res = Hash_Find (symbols, s))) {
            [self release];
            return res;
    } else {
            Hash_Add (symbols, self);
            return self;
    }
}

- (string) printForm
{
    return [self stringValue];
}


@end
