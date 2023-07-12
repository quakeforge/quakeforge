/*
	heapsort.h

	Priority heap related functions

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

#ifndef __QF_heapsort_h
#define __QF_heapsort_h

#include <stddef.h>

#ifndef __compar_fn_t_defined
#define __compar_fn_t_defined
typedef int (*__compar_fn_t)(const void *, const void *);
#endif

#ifndef __compar_d_fn_t_defined
#define __compar_d_fn_t_defined
typedef int (*__compar_d_fn_t)(const void *, const void *, void *);
#endif

#ifndef __swap_d_fn_t_defined
#define __swap_d_fn_t_defined
typedef void (*__swap_d_fn_t)(void *, void *, void *);
#endif

void heap_sink (void *base, size_t ind, size_t nmemb, size_t size,
				__compar_fn_t cmp);
void heap_sink_r (void *base, size_t ind, size_t nmemb, size_t size,
				  __compar_d_fn_t cmp, void *arg);
void heap_sink_s (void *base, size_t ind, size_t nmemb, size_t size,
				  __compar_d_fn_t cmp, __swap_d_fn_t swp, void *arg);

void heap_swim (void *base, size_t ind, size_t nmemb, size_t size,
				__compar_fn_t cmp);
void heap_swim_r (void *base, size_t ind, size_t nmemb, size_t size,
				  __compar_d_fn_t cmp, void *arg);
void heap_swim_s (void *base, size_t ind, size_t nmemb, size_t size,
				  __compar_d_fn_t cmp, __swap_d_fn_t swp, void *arg);

void heap_build (void *base, size_t nmemb, size_t size, __compar_fn_t cmp);
void heap_build_r (void *base, size_t nmemb, size_t size,
				   __compar_d_fn_t cmp, void *arg);
void heap_build_s (void *base, size_t nmemb, size_t size,
				   __compar_d_fn_t cmp, __swap_d_fn_t swp, void *arg);

void heapsort (void *base, size_t nmemb, size_t size, __compar_fn_t cmp);
void heapsort_r (void *base, size_t nmemb, size_t size,
				 __compar_d_fn_t cmp, void *arg);
void heapsort_s (void *base, size_t nmemb, size_t size,
				 __compar_d_fn_t cmp, __swap_d_fn_t swp, void *arg);

#endif//__QF_heapsort_h
