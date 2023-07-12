#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <limits.h>
#include <string.h>

#include "QF/heapsort.h"
#include "QF/qtypes.h"

static inline void
swap_elements (void *a, void *b, size_t size)
{
	byte        tmp[size];
	memcpy (tmp, a, size);
	memcpy (a, b, size);
	memcpy (b, tmp, size);
}

VISIBLE void
heap_sink (void *base, size_t ind, size_t nmemb, size_t size,
		 __compar_fn_t cmp)
{
	size_t      left_ind = 2 * ind + 1;
	size_t      right_ind = 2 * ind + 2;
	void       *node = (byte *) base + ind * size;
	void       *left = (byte *) base + left_ind * size;
	void       *right = (byte *) base + right_ind * size;

	size_t      largest_ind = ind;
	void       *largest = node;

	if (left_ind < nmemb && cmp (left, node) > 0) {
		largest = left;
		largest_ind = left_ind;
	}
	if (right_ind < nmemb && cmp (right, largest) > 0) {
		largest = right;
		largest_ind = right_ind;
	}
	if (largest_ind != ind) {
		swap_elements (largest, node, size);
		heap_sink (base, largest_ind, nmemb, size, cmp);
	}
}

VISIBLE void
heap_sink_r (void *base, size_t ind, size_t nmemb, size_t size,
			 __compar_d_fn_t cmp, void *arg)
{
	size_t      left_ind = 2 * ind + 1;
	size_t      right_ind = 2 * ind + 2;
	void       *node = (byte *) base + ind * size;
	void       *left = (byte *) base + left_ind * size;
	void       *right = (byte *) base + right_ind * size;

	size_t      largest_ind = ind;
	void       *largest = node;

	if (left_ind < nmemb && cmp (left, node, arg) > 0) {
		largest = left;
		largest_ind = left_ind;
	}
	if (right_ind < nmemb && cmp (right, largest, arg) > 0) {
		largest = right;
		largest_ind = right_ind;
	}
	if (largest_ind != ind) {
		swap_elements (largest, node, size);
		heap_sink_r (base, largest_ind, nmemb, size, cmp, arg);
	}
}

VISIBLE void
heap_sink_s (void *base, size_t ind, size_t nmemb, size_t size,
			 __compar_d_fn_t cmp, __swap_d_fn_t swp, void *arg)
{
	size_t      left_ind = 2 * ind + 1;
	size_t      right_ind = 2 * ind + 2;
	void       *node = (byte *) base + ind * size;
	void       *left = (byte *) base + left_ind * size;
	void       *right = (byte *) base + right_ind * size;

	size_t      largest_ind = ind;
	void       *largest = node;

	if (left_ind < nmemb && cmp (left, node, arg) > 0) {
		largest = left;
		largest_ind = left_ind;
	}
	if (right_ind < nmemb && cmp (right, largest, arg) > 0) {
		largest = right;
		largest_ind = right_ind;
	}
	if (largest_ind != ind) {
		swp (largest, node, arg);
		heap_sink_s (base, largest_ind, nmemb, size, cmp, swp, arg);
	}
}

VISIBLE void
heap_swim (void *base, size_t ind, size_t nmemb, size_t size,
		   __compar_fn_t cmp)
{
	size_t      parent_ind = (ind - 1) / 2;
	void       *node = (byte *) base + ind * size;
	void       *parent = (byte *) base + parent_ind * size;

	if (ind > 0 && cmp (node, parent) > 0) {
		swap_elements (node, parent, size);
		heap_swim (base, parent_ind, nmemb, size, cmp);
	}
}

VISIBLE void
heap_swim_r (void *base, size_t ind, size_t nmemb, size_t size,
			 __compar_d_fn_t cmp, void *arg)
{
	size_t      parent_ind = (ind - 1) / 2;
	void       *node = (byte *) base + ind * size;
	void       *parent = (byte *) base + parent_ind * size;

	if (ind > 0 && cmp (node, parent, arg) > 0) {
		swap_elements (node, parent, size);
		heap_swim_r (base, parent_ind, nmemb, size, cmp, arg);
	}
}

VISIBLE void
heap_swim_s (void *base, size_t ind, size_t nmemb, size_t size,
			 __compar_d_fn_t cmp, __swap_d_fn_t swp, void *arg)
{
	size_t      parent_ind = (ind - 1) / 2;
	void       *node = (byte *) base + ind * size;
	void       *parent = (byte *) base + parent_ind * size;

	if (ind > 0 && cmp (node, parent, arg) > 0) {
		swp (node, parent, arg);
		heap_swim_s (base, parent_ind, nmemb, size, cmp, swp, arg);
	}
}

VISIBLE void
heap_build (void *base, size_t nmemb, size_t size, __compar_fn_t cmp)
{
	if (nmemb < 2) {
		return;
	}
	for (size_t i = nmemb / 2; i-- > 0; ) {
		heap_sink (base, i, nmemb, size, cmp);
	}
}

VISIBLE void
heap_build_r (void *base, size_t nmemb, size_t size,
			  __compar_d_fn_t cmp, void *arg)
{
	if (nmemb < 2) {
		return;
	}
	for (size_t i = nmemb / 2; i-- > 0; ) {
		heap_sink_r (base, i, nmemb, size, cmp, arg);
	}
}

VISIBLE void
heap_build_s (void *base, size_t nmemb, size_t size,
			  __compar_d_fn_t cmp, __swap_d_fn_t swp, void *arg)
{
	if (nmemb < 2) {
		return;
	}
	for (size_t i = nmemb / 2; i-- > 0; ) {
		heap_sink_s (base, i, nmemb, size, cmp, swp, arg);
	}
}

VISIBLE void
heapsort (void *base, size_t nmemb, size_t size, __compar_fn_t cmp)
{
	heap_build (base, nmemb, size, cmp);
	for (size_t i = nmemb; i-- > 1; ) {
		void       *last = (byte *) base + i * size;
		swap_elements (base, last, size);
		heap_sink (base, 0, i, size, cmp);
	}
}

VISIBLE void
heapsort_r (void *base, size_t nmemb, size_t size, __compar_d_fn_t cmp,
			void *arg)
{
	heap_build_r (base, nmemb, size, cmp, arg);
	for (size_t i = nmemb; i-- > 1; ) {
		void       *last = (byte *) base + i * size;
		swap_elements (base, last, size);
		heap_sink_r (base, 0, i, size, cmp, arg);
	}
}

VISIBLE void
heapsort_s (void *base, size_t nmemb, size_t size, __compar_d_fn_t cmp,
			__swap_d_fn_t swp, void *arg)
{
	heap_build_s (base, nmemb, size, cmp, swp, arg);
	for (size_t i = nmemb; i-- > 1; ) {
		void       *last = (byte *) base + i * size;
		swp (base, last, arg);
		heap_sink_s (base, 0, i, size, cmp, swp, arg);
	}
}
