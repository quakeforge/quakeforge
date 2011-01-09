#ifndef __Lambda_h
#define __Lambda_h
#include "Procedure.h"
#include "Cons.h"
#include "Frame.h"
#include "CompiledCode.h"

@interface Lambda: Procedure
{
    Frame []env;
    CompiledCode []code;
}
+ (id) newWithCode: (CompiledCode []) c environment: (Frame []) e;
- (id) initWithCode: (CompiledCode []) c environment: (Frame []) e;
@end

#endif //__Lambda_h
