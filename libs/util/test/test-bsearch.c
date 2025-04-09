/*
	test-bsearch.c

	Fuzzy and reentrant bsearch

	Copyright (C) 2021  Bill Currie <bill@taniwha.org>

	This program is free software; you can redistribute it and/or
	modify it under the terms of the GNU General Public License
	as published by the Free Software Foundation; either version 2
	of the License, or (at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "QF/fbsearch.h"
#include "bsearch.h"

// will be prefix-summed
int srcdata[] = {
	2, 1, 3, 1, 2, 2, 5, 4,
	2, 1, 3, 1, 2, 2, 5, 4,
};
#define nele ((int) (sizeof (srcdata) / sizeof (srcdata[0])))
int *data;

static int
compare (const void *a, const void *b)
{
	return *(const int *)a - *(const int *)b;
}

static int
compared (const void *a, const void *b, void *d)
{
	return *(const int *)a - *(const int *)b;
}

static int
fuzzy_check (const int *p, int val, const char *func)
{
	if (val < data[0] && p) {
		printf ("%s found %d, but should not\n", func, val);
		return 0;
	}
	if (p) {
		int         ok = p < data + nele - 1;
		if (p >= data + nele) {
			printf ("%s went out of bounts: %zd\n", func, p - data);
			return 0;
		}
		if (val < *p || (ok && val >= p[1])) {
			printf ("%s found %d:%d for %d\n", func, *p, ok ? p[1] : -1, val);
			return 0;
		}
	}
	if (!p && val >= data[0]) {
		printf ("%s did not find %d, but should have\n", func, val);
		return 0;
	}
	return 1;
}

int
main(int argc, const char **argv)
{
	int         ret = 0;
	int        *p;

	data = malloc (nele * sizeof (int));
	memcpy (data, srcdata, nele * sizeof (int));

	for (int i = 1; i < nele; i++) {
		data[i] += data[i - 1];
	}
	for (int i = 0; i < nele; i++) {
		printf ("%2d %d\n", i, data[i]);
	}
	for (int i = 0; i < data[nele - 1] + 2; i++) {
		p = fbsearch (&i, data, nele, sizeof (int), compare);
		if (!fuzzy_check (p, i, "fbsearch")) {
			ret |= 1;
		}
		p = fbsearch_r (&i, data, nele, sizeof (int), compared, 0);
		if (!fuzzy_check (p, i, "fbsearch_r")) {
			ret |= 1;
		}
		if (p && i == *p) {
			p = QF_bsearch_r (&i, data, nele, sizeof (int), compared, 0);
			if (!p || *p != i) {
				printf ("QF_bsearch_r did not find %d, but should have\n", i);
				ret |= 1;
			}
		} else {
			p = QF_bsearch_r (&i, data, nele, sizeof (int), compared, 0);
			if (p) {
				printf ("QF_bsearch_r found %d, but should not have\n", i);
				ret |= 1;
			}
		}
	}
	free (data);
	return ret;
}
