#ifndef __util_h
#define __util_h

#include <stdint.h>

typedef struct strset_s strset_t;

int count_strings (const char * const *str);
void merge_strings (const char **out, const char * const *in1,
					const char * const *in2);
void prune_strings (strset_t *strset, const char **strings, uint32_t *count);

strset_t *new_strset (const char * const *strings);
void del_strset (strset_t *strset);
void strset_add (strset_t *strset, const char *str);
int strset_contains (strset_t *strset, const char *str);

#endif//__util_h
