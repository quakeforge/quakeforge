#ifndef __Parser_h
#define __Parser_h
#include "Lexer.h"

@interface Parser: Object
{
    Lexer lexer;
}
+ (id) newFromSource: (string) s;
- (id) initWithSource: (string) s;
- (SchemeObject) read;
@end

#endif //__Parser_h
