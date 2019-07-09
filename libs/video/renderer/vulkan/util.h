#ifndef __util_h
#define __util_h

#include <stdint.h>
int count_strings (const char * const *str);
void merge_strings (const char **out, const char * const *in1,
					const char * const *in2);
void prune_strings (const char * const *reference, const char **strings,
					uint32_t *count);

#endif//__util_h
