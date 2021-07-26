/*
	fatpvs.c

	PVS PHS generator tool

	Copyright (C) 2021 Bil Currie

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
		Boston, MA	02111-1307, USA

*/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdlib.h>

#include "QF/bspfile.h"
#include "QF/set.h"
#include "QF/sys.h"

#include "tools/qfvis/include/options.h"
#include "tools/qfvis/include/vis.h"

static set_pool_t *set_pool;
static set_t *base_pvs;
static set_t *fat_pvs;

static unsigned num_leafs;
static unsigned work_leaf;

static int
print_progress (int prev_prog, int spinner_ind)
{
	int         prog;
	prog = work_leaf * 50 / num_leafs;
	if (prog > prev_prog)
		printf ("%.*s", prog - prev_prog, progress + prev_prog);
	printf (" %c\b\b", spinner[spinner_ind % 4]);
	fflush (stdout);
	return prog;
}

static unsigned
next_leaf (void)
{
	unsigned    leaf = ~0;
	WRLOCK (global_lock);
	if (work_leaf < num_leafs) {
		leaf = work_leaf++;
	}
	UNLOCK (global_lock);
	return leaf;
}

static inline void
decompress_vis (const byte *in, unsigned numleafs, set_t *pvs)
{
	byte       *out = (byte *) pvs->map;
	byte       *start = out;
	int			row, c;

	row = (numleafs + 7) >> 3;

	if (!in) {							// no vis info, so make all visible
		while (row) {
			*out++ = 0xff;
			row--;
		}
		return;
	}

	do {
		if (*in) {
			*out++ = *in++;
			continue;
		}

		c = in[1];
		in += 2;
		while (c) {
			*out++ = 0;
			c--;
		}
	} while (out - start < row);
}

static void *
decompress_thread (void *d)
{
	int         thread = (intptr_t) d;
	int64_t     count = 0;

	while (1) {
		unsigned    leaf_num = next_leaf ();

		if (working)
			working[thread] = leaf_num;
		if (leaf_num == ~0u) {
			return 0;
		}
		byte       *visdata = 0;
		dleaf_t    *leaf = &bsp->leafs[leaf_num];
		if (leaf->visofs >= 0) {
			visdata = bsp->visdata + leaf->visofs;
		}
		decompress_vis (visdata, num_leafs, &base_pvs[leaf_num]);
		if (leaf_num == 0) {
			continue;
		}
		for (set_iter_t *iter = set_first_r (&set_pool[thread],
											 &base_pvs[leaf_num]);
			 iter;
			 iter = set_next_r (&set_pool[thread], iter)) {
			count++;
		}
	}
}

static void *
fatten_thread (void *d)
{
	int         thread = (intptr_t) d;
	int64_t     count = 0;

	while (1) {
		unsigned    leaf_num = next_leaf ();

		if (working)
			working[thread] = leaf_num;
		if (leaf_num == ~0u) {
			return 0;
		}

		set_assign (&fat_pvs[leaf_num], &base_pvs[leaf_num]);
		for (set_iter_t *iter = set_first_r (&set_pool[thread],
											 &base_pvs[leaf_num]);
			 iter;
			 iter = set_next_r (&set_pool[thread], iter)) {

			// or this pvs row into the fat pvs
			// +1 because pvs is 1 based
			set_union (&fat_pvs[leaf_num], &base_pvs[iter->element + 1]);
		}
		if (leaf_num == 0) {
			continue;
		}
		for (set_iter_t *iter = set_first_r (&set_pool[thread],
											 &base_pvs[leaf_num]);
			 iter;
			 iter = set_next_r (&set_pool[thread], iter)) {
			count++;
		}
	}
}
#if 0
static void *
fatten_thread2 (void *d)
{
	for (unsigned visword = 0; visword < SET_WORDS (&sv.pvs[0]); visword++) {
		Sys_Printf ("%d / %ld\n", visword, SET_WORDS (&sv.pvs[0]));
		for (i = 0; i < num; i++) {
			for (unsigned j = 0; j < SET_BITS; j++) {
				unsigned    visbit = visword * SET_BITS + j;
				if (visbit >= (unsigned) num) {
					break;
				}
				if (SET_TEST_MEMBER (&sv.pvs[i], visbit)) {
					// or this pvs row into the phs
					// +1 because pvs is 1 based
					set_union (&sv.phs[i], &sv.pvs[visbit + 1]);
				}
			}
		}
	}
	for (i = 1; i < num; i++) {
		for (set_iter_t *iter = set_first (&sv.phs[i]); iter;
			 iter = set_next (iter)) {
			count++;
		}
	}
}
#endif
void
CalcFatPVS (void)
{
	set_pool = calloc (options.threads, sizeof (set_pool_t));
	num_leafs = bsp->models[0].visleafs;
	base_pvs = malloc (num_leafs * sizeof (set_t));
	fat_pvs = malloc (num_leafs * sizeof (set_t));
	for (unsigned i = 0; i < num_leafs; i++) {
		base_pvs[i] = (set_t) SET_STATIC_INIT (num_leafs, malloc);
		fat_pvs[i] = (set_t) SET_STATIC_INIT (num_leafs, malloc);
	}
	work_leaf = 0;
	RunThreads (decompress_thread, print_progress);
	work_leaf = 0;
	RunThreads (fatten_thread, print_progress);
}
