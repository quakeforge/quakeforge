#ifndef __qwaq_debugger_typeencodings_h
#define __qwaq_debugger_typeencodings_h

#include <types.h>
#include <Object.h>

#include "ruamoko/qwaq/debugger/debug.h"

@interface TypeEncodings : Object
+(qfot_type_t *)getType:(unsigned)typeAddr fromTarget:(qdb_target_t)target;
+(int)typeSize:(qfot_type_t *)type;
@end

#endif//__qwaq_debugger_typeencodings_h
