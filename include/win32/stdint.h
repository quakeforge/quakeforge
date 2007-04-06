/* stub for compilers not supporting stdint.h */

#ifdef _MSC_VER

#include <stddef.h> /* brings in intptr_t and uintptr_t */

typedef __int8 int8_t;
typedef __int16 int16_t;
typedef __int32 int32_t;
typedef __int64 int64_t;

typedef unsigned __int8 uint8_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int64 uint64_t;

/* TODO: do we need int_leastX_t? */

#endif
