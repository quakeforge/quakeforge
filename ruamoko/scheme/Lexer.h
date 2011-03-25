#ifndef __Lexer_h
#define __Lexer_h
#include "SchemeObject.h"
#include "Symbol.h"

@interface Lexer: SchemeObject
{
    string source;
    string filename;
    int linenum;
}
+ (id) newFromSource: (string) s file: (string) f;
- (id) initWithSource: (string) s file: (string) f;
- (SchemeObject *) nextToken;
- (int) lineNumber;
@end

#endif //__Lexer_h
