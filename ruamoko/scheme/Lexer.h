#ifndef __Lexer_h
#define __Lexer_h
#include "SchemeObject.h"
#include "Symbol.h"

@interface Lexer: Object
{
    string source;
    Symbol lparen;
    Symbol rparen;
}
+ (id) newFromSource: (string) s;
- (id) initWithSource: (string) s;
- (SchemeObject) nextToken;
@end

#endif //__Lexer_h
