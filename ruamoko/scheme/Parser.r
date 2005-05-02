#include "Cons.h"
#include "Parser.h"
#include "Nil.h"
#include "defs.h"

@implementation Parser

+ (id) newFromSource: (string) s
{
    return [[self alloc] initWithSource: s];
}

- (id) initWithSource: (string) s
{
    self = [super init];
    lexer = [Lexer newFromSource: s];
    return self;
}

- (SchemeObject) readList
{
    local SchemeObject token;
    
    token = [self read];

    if (!token)
            return NIL;
    
    if (token == [Symbol rightParen]) {
            return [Nil nil];
    } else {
            return [Cons newWithCar: token cdr: [self readList]];
    }
}

- (SchemeObject) read
{
    local SchemeObject token;
    local SchemeObject list;
    
    token = [lexer nextToken];

    if (!token) {
            return NIL;
    }

    
    if (token == [Symbol leftParen]) {
            list = [self readList];
            return list;
    } else if (token == [Symbol quote]) {
            return cons([Symbol forString: "quote"], cons([self read], [Nil nil]));
    } else return token;
}

@end
