#include <stdlib.h>

void *bsearch (void *key, void *array, int nmemb, int size,
			   int (*compare) (void *a, void *b)) = #0;
void *fbsearch (void *key, void *array, int nmemb, int size,
			    int (*compare) (void *a, void *b)) = #0;
void *qsort (void *array, int nmemb, int size,
			 int (*compare) (void *a, void *b)) = #0;

void prefixsum (int *array, int count) = #0;
void prefixsum (float *array, int count) = #0;
