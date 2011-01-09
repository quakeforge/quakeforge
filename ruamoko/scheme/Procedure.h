#ifndef __Procedure_h
#define __Procedure_h
#include "SchemeObject.h"

@class Machine;

@interface Procedure: SchemeObject
- (void) invokeOnMachine: (Machine []) m;
@end

#endif //__Procedure_h
