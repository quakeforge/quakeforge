#ifndef __stdlib_h
#define __stdlib_h

//FIXME add const to qfcc?
// If compare is nil, the value pointed to by key and the first member of the
// elements in the array are treated as ints, making for fast searches when the
// first member of the element is the key and is a simple int
void *bsearch (void *key, void *array, int nmemb, int size,
			   int (*compare) (void *a, void *b));
void *fbsearch (void *key, void *array, int nmemb, int size,
			    int (*compare) (void *a, void *b));
void *qsort (void *array, int nmemb, int size,
			 int (*compare) (void *a, void *b));
@overload void prefixsum (int *array, int count);
@overload void prefixsum (float *array, int count);

#endif//__stdlib_h
