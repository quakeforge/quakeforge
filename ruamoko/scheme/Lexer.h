#ifndef __Lexer_h
#define __Lexer_h
#include "SchemeObject.h"
#include "Symbol.h"

@interface Lexer: Object
{
    string source;
    string filename;
    integer linenum;
}
+ (id) newFromSource: (string) s file: (string) f;
- (id) initWithSource: (string) s file: (string) f;
- (SchemeObject) nextToken;
- (integer) lineNumber;
@end

#endif //__Lexer_h
