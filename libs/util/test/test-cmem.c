#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "QF/cmem.h"
#include "QF/set.h"

static int
test_block (memsuper_t *super)
{
	size_t      size = super->page_size;
	void       *mem = cmemalloc (super, size);
	memblock_t *block;

	if (!mem) {
		fprintf (stderr, "could not allocate %zd byte block\n",
				 super->page_size);
		return 0;
	}
	if ((size_t) mem & super->page_mask) {
		fprintf (stderr, "mem not page aligned: %p %zd\n",
				 mem, super->page_size);
		return 0;
	}
	block = super->memblocks;
	if (mem != block + 1) {
		fprintf (stderr, "super does not point to mem\n");
		return 0;
	}
	if (block->post_size < size) {
		fprintf (stderr, "block post_size too small: %zd < %zd\n",
				 block->post_size, size);
		return 0;
	}
	if (block->post_size - size >= super->page_size) {
		fprintf (stderr, "block post_size too big: %zd < %zd\n",
				 block->post_size - size, super->page_size);
		return 0;
	}
	memset (mem, 0, size);	// valgrind check
	cmemfree (super, mem);
	if (super->memblocks) {
		fprintf (stderr, "super still points to mem\n");
		return 0;
	}
	return 1;
}

static int
check_block (memblock_t *block, int line_count, int allocated)
{
	set_t      *visited = set_new ();
	size_t      free_bytes = 0;
	int         ret = 1;
	int         count = 0;

	for (memline_t **l = &block->free_lines; *l; l = &(*l)->block_next) {
		memline_t  *line = *l;
		ptrdiff_t   ind = (byte *) block - (byte *) line;
		ind /= MEM_LINE_SIZE;
		if (ind < 1 || (size_t ) ind > block->pre_size / MEM_LINE_SIZE) {
			fprintf (stderr, "line outside of block: %p %p %p\n",
					 line, block->mem, block);
			return 0;
		}
		if (set_is_member (visited, ind)) {
			fprintf (stderr, "loop in block free_lines\n");
			return 0;
		}
		count++;
		set_add (visited, ind);
		if (!line->size) {
			fprintf (stderr, "line with size 0\n");
			ret = 0;
		}
		if (line->block_next && line->block_next < line) {
			fprintf (stderr, "line link in wrong direction\n");
			ret = 0;
		}
		if ((size_t) line + line->size == (size_t) line->block_next) {
			fprintf (stderr, "adjacant free line blocks\n");
			ret = 0;
		}
		if (line->block_prev != l) {
			fprintf (stderr, "line block_prev link incorrect\n");
			ret = 0;
		}
		if (line->size > block->pre_size) {
			fprintf (stderr, "line size too large: %zd / %zd\n",
					 line->size, block->pre_size);
			ret = 0;
		}
		if (line->size % MEM_LINE_SIZE) {
			fprintf (stderr, "bad line size: %zd / %zd\n",
					 line->size, line->size % MEM_LINE_SIZE);
			ret = 0;
		}
		if (ret) {
			free_bytes += line->size;
		}
	}
	if (ret) {
		if (free_bytes + block->pre_allocated != block->pre_size) {
			fprintf (stderr, "block space mismatch: s: %zd a: %zd f: %zd\n",
					 block->pre_size, block->pre_allocated, free_bytes);
			ret = 0;
		}
		if (line_count >= 0 && line_count != count) {
			fprintf (stderr, "incorrect number of lines: e: %d g: %d\n",
					 line_count, count);
			ret = 0;
		}
		if (allocated >= 0 && (size_t) allocated != block->pre_allocated) {
			fprintf (stderr, "pre_allocated wrong size: %zd != %d\n",
					 block->pre_allocated, allocated);
		}
	}
	set_delete (visited);
	return ret;
}

static int __attribute__ ((pure))
check_for_loop (memline_t *line, memline_t **stop)
{
	memline_t  *next = line->free_next;

	for (memline_t **l = line->free_prev; l != stop;
		 l = line->free_prev) {
		line = (memline_t *) l;
		if (line == next) {
			return 1;
		}
	}
	return 0;
}

static int
check_bins (memsuper_t *super, int mask)
{
	int ret = 1;
	for (int i = MAX_CACHE_LINES; i-- > 0; ) {
		if (mask >= 0) {
			if (mask & (1 << i)) {
				if (!super->free_lines[i]) {
					fprintf (stderr, "super free_lines[%d] is empty\n", i);
					ret = 0;
				}
			} else {
				if (super->free_lines[i]) {
					fprintf (stderr, "super free_lines[%d] is occupied\n", i);
					ret = 0;
				}
			}
		}
		for (memline_t **l = &super->free_lines[i]; *l; l = &(*l)->free_next) {
			memline_t  *line = *l;
			if (line->free_prev != l) {
				fprintf (stderr, "super free_lines[%d] has bad prev\n", i);
				ret = 0;
				break;
			}
			if (check_for_loop (line, &super->free_lines[i])) {
				fprintf (stderr, "super free_lines[%d] loop detected\n", i);
				ret = 0;
				break;
			}
			if ((MEM_LINE_SIZE << i) > (int) line->size
				|| (MEM_LINE_SIZE << (i + 1)) <= (int) line->size) {
				fprintf (stderr, "line in wrong i: %d %d %zd\n",
						 i, MEM_LINE_SIZE << i, line->size);
				ret = 0;
			}
		}
	}
	return ret;
}

static int
test_line (memsuper_t *super)
{
	memline_t  *line1 = cmemalloc (super, MEM_LINE_SIZE);
	memline_t  *line2 = cmemalloc (super, MEM_LINE_SIZE);
	memline_t  *line3 = cmemalloc (super, MEM_LINE_SIZE);
	memblock_t *block = super->memblocks;

	if (block->next) {
		fprintf (stderr, "too many memblocks\n");
		return 0;
	}
	if (line1 < (memline_t *) block->mem || line1 >= (memline_t *) block) {
		fprintf (stderr, "line1 outside block line pool: %p %p %p\n",
				 line1, block->mem, block);
		return 0;
	}
	if (line2 < (memline_t *) block->mem || line2 >= (memline_t *) block) {
		fprintf (stderr, "line2 outside block line pool: %p %p %p\n",
				 line2, block->mem, block);
		return 0;
	}
	if (line3 < (memline_t *) block->mem || line3 >= (memline_t *) block) {
		fprintf (stderr, "line3 outside block line pool: %p %p %p\n",
				 line3, block->mem, block);
		return 0;
	}
	if (!((size_t) line1 & super->page_mask)) {
		fprintf (stderr, "line1 is page aligned\n");
		return 0;
	}
	if (!((size_t) line2 & super->page_mask)) {
		fprintf (stderr, "line2 is page aligned\n");
		return 0;
	}
	if (!((size_t) line3 & super->page_mask)) {
		fprintf (stderr, "line3 is page aligned\n");
		return 0;
	}
	if (line1 + 1 != line2 || line2 + 1 != line3) {
		fprintf (stderr, "lines not contiguous\n");
		return 0;
	}
	if (line3 + 1 != block->free_lines) {
		fprintf (stderr, "line3 not contiguous with free lines\n");
		return 0;
	}

	if (!check_block (block, 1, 3 * MEM_LINE_SIZE)) {
		fprintf (stderr, "line block check 1 failed\n");
		return 0;
	}
	if (!check_bins (super, 0x20)) {
		fprintf (stderr, "bin check 1 failed\n");
		return 0;
	}

	cmemfree (super, line2);

	if (!check_block (block, 2, 2 * MEM_LINE_SIZE)) {
		fprintf (stderr, "line block check 2 failed\n");
		return 0;
	}
	if (!check_bins (super, 0x21)) {
		fprintf (stderr, "bin check 2 failed\n");
		return 0;
	}

	if (block->free_lines != line2) {
		fprintf (stderr, "block free_lines not pointing to line2\n");
		return 0;
	}
	if (super->free_lines[0] != line2) {
		fprintf (stderr, "super free_lines[0] not pointing to line2\n");
		return 0;
	}

	cmemfree (super, line3);
	if (!check_block (block, 1, 1 * MEM_LINE_SIZE)) {
		fprintf (stderr, "line block check 3 failed\n");
		return 0;
	}
	if (!check_bins (super, 0x20)) {
		fprintf (stderr, "bin check 3 failed\n");
		return 0;
	}

	if (block->free_lines != line2) {
		fprintf (stderr, "free lines not pointing to line2 2\n");
		return 0;
	}

	cmemfree (super, line1);
	if (super->memblocks) {
		fprintf (stderr, "line pool not freed\n");
		return 0;
	}
	if (!check_bins (super, 0x00)) {
		fprintf (stderr, "bins not cleared\n");
		return 0;
	}

	line1 = cmemalloc (super, MEM_LINE_SIZE);
	line2 = cmemalloc (super, MEM_LINE_SIZE);
	line3 = cmemalloc (super, MEM_LINE_SIZE);
	block = super->memblocks;

	if (!check_block (block, 1, 3 * MEM_LINE_SIZE)) {
		fprintf (stderr, "line block check 4 failed\n");
		return 0;
	}
	if (!check_bins (super, 0x20)) {
		fprintf (stderr, "bin check 4 failed\n");
		return 0;
	}

	cmemfree (super, line1);

	if (!check_block (block, 2, 2 * MEM_LINE_SIZE)) {
		fprintf (stderr, "line block check 5 failed\n");
		return 0;
	}
	if (!check_bins (super, 0x21)) {
		fprintf (stderr, "bin check 5 failed\n");
		return 0;
	}

	cmemfree (super, line2);

	if (!check_block (block, 2, 1 * MEM_LINE_SIZE)) {
		fprintf (stderr, "line block check 6 failed\n");
		return 0;
	}
	if (!check_bins (super, 0x22)) {
		fprintf (stderr, "bin check 6 failed\n");
		return 0;
	}

	cmemfree (super, line3);
	if (super->memblocks) {
		fprintf (stderr, "line pool not freed 2\n");
		return 0;
	}
	if (!check_bins (super, 0x00)) {
		fprintf (stderr, "bins not cleared 2\n");
		return 0;
	}

	line1 = cmemalloc (super, MEM_LINE_SIZE);
	line2 = cmemalloc (super, MEM_LINE_SIZE);
	line3 = cmemalloc (super, MEM_LINE_SIZE);
	block = super->memblocks;

	if (!check_block (block, 1, 3 * MEM_LINE_SIZE)) {
		fprintf (stderr, "line block check 7 failed\n");
		return 0;
	}
	if (!check_bins (super, 0x20)) {
		fprintf (stderr, "bin check 7 failed\n");
		return 0;
	}

	cmemfree (super, line3);

	if (!check_block (block, 1, 2 * MEM_LINE_SIZE)) {
		fprintf (stderr, "line block check 8 failed\n");
		return 0;
	}
	if (!check_bins (super, 0x20)) {
		fprintf (stderr, "bin check 8 failed\n");
		return 0;
	}

	cmemfree (super, line2);

	if (!check_block (block, 1, 1 * MEM_LINE_SIZE)) {
		fprintf (stderr, "line block check 9 failed\n");
		return 0;
	}
	if (!check_bins (super, 0x20)) {
		fprintf (stderr, "bin check 9 failed\n");
		return 0;
	}

	cmemfree (super, line1);
	if (super->memblocks) {
		fprintf (stderr, "line pool not freed 3\n");
		return 0;
	}
	if (!check_bins (super, 0x00)) {
		fprintf (stderr, "bins not cleared 3\n");
		return 0;
	}

	return 1;
}

typedef struct {
	size_t      size;
	int         group_id;
} sline_block_t;

static sline_block_t group_tests[] = {
	{ 2,  4},
	{ 4,  4},
	{ 5,  8},
	{ 3,  4},
	{ 1,  4},
	{ 6,  8},
	{ 9, 16},
	{ 4,  4},
	{ 4,  4},
	{ 7,  8},
	{ 2,  4},
	{ 4,  4},
	{ 8,  8},
	{13, 16},
	{ 3,  4},
	{ 1,  4},
	{ 8,  8},
	{ 8,  8},
	{ 4,  4},
	{ 4,  4},
	{16, 16},
	{32, 32},
	{32, 33},
};
#define num_group_tests (sizeof (group_tests) / sizeof (group_tests[0]))
#define mask(x) (((size_t) (x)) & ~(MEM_LINE_SIZE - 1))
#define pagemask(x,o,s) (((size_t) (x)) & (o (s)->page_mask))

static int
test_sline (memsuper_t *super)
{
	void       *mem[num_group_tests];
	int         ret = 1;

	for (size_t i = 0; i < num_group_tests; i++) {
		mem[i] = cmemalloc (super, group_tests[i].size);
		if (!mem[i]) {
			fprintf (stderr, "mem[%zd] is null\n", i);
			return 0;
		}
		if (!((size_t) mem[i] % MEM_LINE_SIZE)) {
			fprintf (stderr, "mem[%zd] is aligned with chache line\n", i);
			ret = 0;
		}
	}
	for (size_t i = 0; i < num_group_tests; i++) {
		for (size_t j = i + 1; j < num_group_tests; j++) {
			if (mem[i] == mem[j]) {
				fprintf (stderr, "mem[%zd] is dupped with %zd\n", i, j);
				ret = 0;
			}
			if (mask (mem[i]) == mask (mem[j])) {
				if (group_tests[i].group_id != group_tests[j].group_id) {
					fprintf (stderr, "mem[%zd](%d) is grouped with %zd(%d)\n",
							 i, group_tests[i].group_id,
							 j, group_tests[j].group_id);
					ret = 0;
				}
			} else {
				if (group_tests[i].group_id == group_tests[j].group_id) {
					fprintf (stderr,
							 "mem[%zd](%d) is not grouped with %zd(%d)\n",
							 i, group_tests[i].group_id,
							 j, group_tests[j].group_id);
					ret = 0;
				}
			}
			if (pagemask (mem[i], ~, super) != pagemask (mem[j], ~, super)) {
				fprintf (stderr,
						 "mem[%zd](%d) is not block grouped with %zd(%d)\n",
						 i, group_tests[i].group_id,
						 j, group_tests[j].group_id);
			}
		}
	}
	for (size_t i = 0; i < num_group_tests; i++) {
		cmemfree (super, mem[i]);
		void   *newmem = cmemalloc (super, group_tests[i].size);
		if (newmem != mem[i]) {
			fprintf (stderr,
					"%2zd bytes not recycled (%2zd,%2d) (%06zx %06zx)\n",
					 group_tests[i].size, i, group_tests[i].group_id,
					 pagemask (mem[i], ~~, super),
					 pagemask (newmem, ~~, super));
			ret = 0;
		}
		if (!((size_t) newmem % MEM_LINE_SIZE)) {
			fprintf (stderr, "newmem is aligned with chache line %p\n",
					 newmem);
			ret = 0;
		}
	}
	return ret;
}

static int
test_block_line (memsuper_t *super)
{
	void       *mem = cmemalloc (super, 2 * super->page_size);
	void       *line;
	memblock_t *block = super->memblocks;

	if (block + 1 != (memblock_t *) mem) {
		fprintf (stderr, "super memblocks do not point to mem\n");
		return 0;
	}
	if (block->pre_size < MEM_LINE_SIZE) {
		// need to figure out a way to guarantee a shared block
		fprintf (stderr, "can't allocate line from block\n");
		return 0;
	}
	if (block->next) {
		fprintf (stderr, "excess blocks in super\n");
		return 0;
	}
	line = cmemalloc (super, MEM_LINE_SIZE);
	if (!((size_t) line & super->page_mask)) {
		fprintf (stderr, "line is page aligned\n");
		return 0;
	}
	if (super->memblocks->next) {
		// need to figure out a way to guarantee a shared block
		fprintf (stderr, "mem and line not in same block\n");
		return 0;
	}
	cmemfree (super, mem);
	if (!super->memblocks) {
		fprintf (stderr, "shared block freed\n");
		return 0;
	}
	if (cmemalloc (super, super->page_size) != mem) {
		fprintf (stderr, "block not reused for mem\n");
		return 0;
	}
	if (super->memblocks != block || super->memblocks->next) {
		// need to figure out a way to guarantee a shared block
		fprintf (stderr, "blocks corrupt\n");
		return 0;
	}
	cmemfree (super, line);
	if (!super->memblocks) {
		fprintf (stderr, "shared block freed 2\n");
		return 0;
	}
	cmemfree (super, mem);
	if (super->memblocks) {
		fprintf (stderr, "shared block not freed\n");
		return 0;
	}
	return 1;
}

int
main (void)
{
	memsuper_t *super = new_memsuper ();
	int         i;
#if __WORDSIZE == 32
	if (sizeof (memsuper_t) != 1 * MEM_LINE_SIZE) {
		fprintf (stderr, "memsuper_t not 2 * cache size: %zd\n",
				 sizeof (memsuper_t));
		return 1;
	}
#else
	if (sizeof (memsuper_t) != 2 * MEM_LINE_SIZE) {
		fprintf (stderr, "memsuper_t not 2 * cache size: %zd\n",
				 sizeof (memsuper_t));
		return 1;
	}
#endif
	if (sizeof (memline_t) != MEM_LINE_SIZE) {
		fprintf (stderr, "memline_t not cache size: %zd\n",
				 sizeof (memline_t));
		return 1;
	}
	if (sizeof (memsline_t) != 2 * sizeof (void *)) {
		fprintf (stderr, "memsline_t not two pointers: %zd\n",
				 sizeof (memsline_t));
		return 1;
	}
	if (sizeof (memblock_t) != MEM_LINE_SIZE) {
		fprintf (stderr, "memblock_t not cache size: %zd\n",
				 sizeof (memblock_t));
		return 1;
	}
	if ((size_t) super & (MEM_LINE_SIZE - 1)) {
		fprintf (stderr, "super block not cache aligned: %p\n", super);
		return 1;
	}
	if (super->page_size != (size_t) sysconf (_SC_PAGESIZE)) {
		fprintf (stderr, "page size not equal to system page size: %zd, %zd\n",
				 super->page_size, (size_t) sysconf (_SC_PAGESIZE));
		return 1;
	}
	if (!super->page_size || (super->page_size & (super->page_size - 1))) {
		fprintf (stderr, "page size not power of two: %zd\n",
				 super->page_size);
		return 1;
	}
	if (super->page_mask + 1 != super->page_size) {
		fprintf (stderr, "page mask not page size - 1: %zx %zx\n",
				 super->page_mask, super->page_size);
		return 1;
	}
	if (!super->page_mask || (super->page_mask & (super->page_mask + 1))) {
		fprintf (stderr, "page mask not all 1s: %zx\n",
				 super->page_mask);
		return 1;
	}
	if (super->memblocks) {
		fprintf (stderr, "super block list not null\n");
		return 1;
	}
	for (i = 4; i-- > 0; ) {
		if (super->last_freed[i]) {
			break;
		}
	}
	if (i >= 0) {
		fprintf (stderr, "super last_freed not all null\n");
		return 1;
	}
	for (i = MAX_CACHE_LINES; i-- > 0; ) {
		if (super->free_lines[i]) {
			break;
		}
	}
	if (i >= 0) {
		fprintf (stderr, "super free_lines not all null\n");
		return 1;
	}
	if (!test_block (super)) {
		fprintf (stderr, "block tests failed\n");
		return 1;
	}
	if (super->memblocks) {
		fprintf (stderr, "super block list not null 2\n");
		return 1;
	}
	for (i = 4; i-- > 0; ) {
		if (super->last_freed[i]) {
			break;
		}
	}
	if (i >= 0) {
		fprintf (stderr, "super last_freed not all null 2\n");
		return 1;
	}
	for (i = MAX_CACHE_LINES; i-- > 0; ) {
		if (super->free_lines[i]) {
			break;
		}
	}
	if (i >= 0) {
		fprintf (stderr, "super free_lines not all null 2\n");
		return 1;
	}
	if (!test_line (super)) {
		fprintf (stderr, "line tests failed\n");
		return 1;
	}
	if (super->memblocks) {
		fprintf (stderr, "super block list not null 2\n");
		return 1;
	}
	if (!test_block_line (super)) {
		fprintf (stderr, "block-line tests failed\n");
		return 1;
	}
	for (size_t i = 0; i < 2 * super->page_size / MEM_LINE_SIZE; i++) {
		void       *line = cmemalloc (super, MEM_LINE_SIZE);
		if (!line) {
			fprintf (stderr, "could not allocate %d byte line\n",
					 MEM_LINE_SIZE);
			return 1;
		}
		if ((size_t) line % MEM_LINE_SIZE) {
			fprintf (stderr, "line not cache-line aligned: %p %d\n",
					 line, MEM_LINE_SIZE);
			return 1;
		}
		if (!((size_t) line & super->page_mask)) {
			fprintf (stderr, "line is page aligned: %p %zd\n",
					 line, super->page_size);
			return 1;
		}
	}
	if (!test_sline (super)) {
		fprintf (stderr, "sub-line tests failed\n");
		return 1;
	}
	delete_memsuper (super);
	return 0;
}
