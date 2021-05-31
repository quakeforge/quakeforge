/*
	fbsearch.h

	Fuzzy bsearch

	Copyright (C) 2021 Bill Currie <bill@taniwha.org>

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

#ifndef __QF_bsearch_h
#define __QF_bsearch_h

#include <stddef.h>

#ifndef __compar_fn_t_defined
#define __compar_fn_t_defined
typedef int (*__compar_fn_t)(const void *, const void *);
#endif

#ifndef __compar_d_fn_t_defined
#define __compar_d_fn_t_defined
typedef int (*__compar_d_fn_t)(const void *, const void *, void *);
#endif

void *fbsearch (const void *key, const void *base, size_t nmemb, size_t size,
				__compar_fn_t cmp);

void *fbsearch_r (const void *key, const void *base, size_t nmemb, size_t size,
				  __compar_d_fn_t cmp, void *arg);

#endif//__QF_bsearch_h
