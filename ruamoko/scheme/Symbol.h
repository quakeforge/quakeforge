#ifndef __Symbol_h
#define __Symbol_h
#include "SchemeString.h"

@interface Symbol: String
{
}
+ (void) initialize;
+ (Symbol *) leftParen;
+ (Symbol *) rightParen;
+ (Symbol *) dot;
+ (Symbol *) forString: (string) s;
+ (Symbol *) quote;
@end

@extern Symbol *symbol (string str);

#endif //__Symbol_h
