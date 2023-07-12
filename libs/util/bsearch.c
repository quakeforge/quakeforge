#include <limits.h>
#include <string.h>

#include "QF/fbsearch.h"
#include "bsearch.h"

void *fbsearch (const void *key, const void *_base, size_t nmemb, size_t size,
				__compar_fn_t cmp)
{
	// fuzzy bsearh
	const char *base = (const char *) _base;
	unsigned    left = 0;
	unsigned    right = nmemb - 1;
	unsigned    mid;
	const void *p = 0;

	if (!nmemb) {
		return 0;
	}
	while (left != right) {
		mid = (left + right + 1) / 2;
		p = base + mid * size;
		if (cmp (key, p) < 0) {
			right = mid - 1;
		} else {
			left = mid;
		}
	}
	p = base + left * size;
	if (cmp (key, p) >= 0) {
		return (void *) p;
	}
	return 0;
}

void *fbsearch_r (const void *key, const void *_base, size_t nmemb, size_t size,
				  __compar_d_fn_t cmp, void *arg)
{
	// fuzzy bsearh
	const char *base = (const char *) _base;
	unsigned    left = 0;
	unsigned    right = nmemb - 1;
	unsigned    mid;
	const void *p = 0;

	if (!nmemb) {
		return 0;
	}
	while (left != right) {
		mid = (left + right + 1) / 2;
		p = base + mid * size;
		if (cmp (key, p, arg) < 0) {
			right = mid - 1;
		} else {
			left = mid;
		}
	}
	p = base + left * size;
	if (cmp (key, p, arg) >= 0) {
		return (void *) p;
	}
	return 0;
}

void *
QF_bsearch_r(const void *key, const void *_base, size_t nmemb, size_t size,
			 __compar_d_fn_t cmp, void *arg)
{
	const char *base = (const char *) _base;
	unsigned    left = 0;
	unsigned    right = nmemb - 1;
	unsigned    mid;
	const void *p = 0;
	int         c;

	if (!nmemb) {
		return 0;
	}
	while (left != right) {
		mid = (left + right + 1) / 2;
		p = base + mid * size;
		c = cmp (key, p, arg);
		if (c == 0) {
			return (void *) p;
		}
		if (c < 0) {
			right = mid - 1;
		} else {
			left = mid;
		}
	}
	p = base + left * size;
	if (cmp (key, p, arg) == 0) {
		return (void *) p;
	}
	return 0;
}
