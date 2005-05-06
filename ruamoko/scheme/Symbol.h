#ifndef __Symbol_h
#define __Symbol_h
#include "String.h"

@interface Symbol: String
{
}
+ (void) initialize;
+ (Symbol) leftParen;
+ (Symbol) rightParen;
+ (Symbol) dot;
+ (Symbol) forString: (string) s;
@end

@extern Symbol symbol (string str);

#endif //__Symbol_h
