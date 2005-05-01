#ifndef __String_h
#define __String_h
#include "SchemeObject.h"

@interface String: SchemeObject
{
    string value;
}
+ (id) newFromString: (string) s;
- (id) initWithString: (string) s;
- (string) stringValue;
@end

#endif //__String_h
