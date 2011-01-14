#include "Cons.h"
#include "Parser.h"
#include "Nil.h"
#include "Error.h"
#include "defs.h"

@implementation Parser

+ (id) newFromSource: (string) s file: (string) f
{
    return [[self alloc] initWithSource: s file: f];
}

- (id) initWithSource: (string) s file: (string) f
{
    self = [super init];
    lexer = [Lexer newFromSource: s file: f];
    file = f;
    return self;
}

- (SchemeObject[]) readList
{
    local SchemeObject []token, res;
    local integer line;
    local Error []err;

    line = [lexer lineNumber];
    token = [self readAtomic];

    if ([token isError])
            return token;
    
    if (!token) {
            err = [Error type: "parse" message: "Unmatched open parenthesis"];
            [err source: file];
            [err line: [lexer lineNumber]];
            return err;
    }
    
    if (token == [Symbol rightParen]) {
            return [Nil nil];
    } else if (token == [Symbol dot]) {
            res = [self readAtomic];
            if ([res isError]) return res;
            if ([self readAtomic] != [Symbol rightParen]) {
                    err = [Error type: "parse" message: "Improper use of dot"];
                    [err source: file];
                    [err line: [lexer lineNumber]];
                    return err;
            }
            return res;
    } else {
            res = [self readList];
            if ([res isError]) return res;
            res = cons(token, res);
            [res source: file];
            [res line: line];
            return res;
    }
}

- (SchemeObject[]) readAtomic
{
    local SchemeObject []token, list, res;
    local integer line;

    line = [lexer lineNumber];
 
    token = [lexer nextToken];

    if ([token isError]) {
            return token;
    }
    
    if (!token) {
            return nil;
    }

    if (token == [Symbol leftParen]) {
            list = [self readList];
            return list;
    } else if (token == [Symbol quote]) {
            res = [self read];
            if ([res isError]) return res;
            res = cons(res, [Nil nil]);
            [res source: file];
            [res line: line];
            res = cons([Symbol forString: "quote"], res);
            [res source: file];
            [res line: line];
            return res;
    } else return token;
}

- (SchemeObject[]) read
{
    local SchemeObject []token;
    local Error []err;

    token = [self readAtomic];
    if (token == [Symbol rightParen]) {
            err = [Error type: "parse" message: "mismatched close parentheis"];
            [err source: file];
            [err line: [lexer lineNumber]];
            return err;
    }
    return token;
}

- (void) markReachable
{
    [lexer mark];
}

@end
