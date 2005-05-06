#ifndef __Parser_h
#define __Parser_h
#include "Lexer.h"

@interface Parser: SchemeObject
{
    Lexer lexer;
    string file;
}
+ (id) newFromSource: (string) s file: (string) f;
- (id) initWithSource: (string) s file: (string) f;
- (SchemeObject) readAtomic;
- (SchemeObject) read;
@end

#endif //__Parser_h
