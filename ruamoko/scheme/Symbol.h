#ifndef __Symbol_h
#define __Symbol_h
#include "String.h"

@interface Symbol: String
{
}
+ (void) initialize;
+ (Symbol) leftParen;
+ (Symbol) rightParen;
+ (Symbol) forString: (string) s;
@end

#endif //__Symbol_h
