#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/cmem.h"

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
		fprintf (stderr, "line1 outside block line pool\n");
		return 0;
	}
	if (line2 < (memline_t *) block->mem || line2 >= (memline_t *) block) {
		fprintf (stderr, "line2 outside block line pool\n");
		return 0;
	}
	if (line3 < (memline_t *) block->mem || line3 >= (memline_t *) block) {
		fprintf (stderr, "line3 outside block line pool\n");
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
	if (block->free_lines->next) {
		fprintf (stderr, "multiple free line blocks\n");
		return 0;
	}
	if (block->pre_allocated != 3 * MEM_LINE_SIZE) {
		fprintf (stderr, "pre_allocated wrong size: %zd != %d\n",
				 block->pre_allocated, 3 * MEM_LINE_SIZE);
		return 0;
	}
	if (block->free_lines->size != block->pre_size - block->pre_allocated) {
		fprintf (stderr, "free lines wrong size: %zd != %zd\n",
				 block->free_lines->size,
				 block->pre_size - block->pre_allocated);
		return 0;
	}
	size_t      old_size = block->free_lines->size;
	memline_t  *old_line = block->free_lines;
	cmemfree (super, line2);
	if (block->pre_allocated != 2 * MEM_LINE_SIZE) {
		fprintf (stderr, "pre_allocated wrong size: %zd != %d\n",
				 block->pre_allocated, 2 * MEM_LINE_SIZE);
		return 0;
	}
	if (block->free_lines != line2) {
		fprintf (stderr, "free lines not pointing to line2\n");
		return 0;
	}
	if (!block->free_lines->next || block->free_lines->next->next) {
		fprintf (stderr, "incorrect number of free blocks\n");
		return 0;
	}
	if (line2->next != old_line || old_line->size != old_size) {
		fprintf (stderr, "free line blocks corrupted\n");
		return 0;
	}
	if (block->free_lines->size != MEM_LINE_SIZE) {
		fprintf (stderr, "free line block wrong size: %zd != %d\n",
				 block->free_lines->size, MEM_LINE_SIZE);
		return 0;
	}
	cmemfree (super, line3);
	if (block->free_lines != line2) {
		fprintf (stderr, "free lines not pointing to line2 2\n");
		return 0;
	}
	if (block->pre_allocated != MEM_LINE_SIZE) {
		fprintf (stderr, "pre_allocated wrong size: %zd != %d\n",
				 block->pre_allocated, MEM_LINE_SIZE);
		return 0;
	}
	if (block->free_lines->size != block->pre_size - block->pre_allocated) {
		fprintf (stderr, "free lines wrong size: %zd != %zd\n",
				 block->free_lines->size,
				 block->pre_size - block->pre_allocated);
		return 0;
	}
	cmemfree (super, line1);
	if (super->memblocks) {
		fprintf (stderr, "line pool not freed\n");
		return 0;
	}
	return 1;
}

static int
test_sline (memsuper_t *super)
{
	void       *mem[] = {
		//cmemalloc (super, 2),	// smaller than min size
		cmemalloc (super, 4),
		cmemalloc (super, 4),
		cmemalloc (super, 8),
		cmemalloc (super, 8),
		cmemalloc (super, 16),
		cmemalloc (super, 16),
		cmemalloc (super, 32),
		cmemalloc (super, 32),
	};
#define mem_size (sizeof (mem) / sizeof (mem[0]))
	int fail = 0;
	for (size_t i = 0; i < mem_size; i++) {
		printf("%p\n", mem[i]);
		if (!mem[i]) {
			fprintf (stderr, "mem[%zd] is null\n", i);
			fail = 1;
		}
		for (size_t j = i + 1; j < mem_size; j++) {
			if (mem[i] == mem[j]) {
				fprintf (stderr, "mem[%zd] is dupped with %zd\n", i, j);
				fail = 1;
			}
		}
	}
	if (fail) {
		return 0;
	}
#undef mem_size
	return 1;
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

	if (sizeof (memsuper_t) != MEM_LINE_SIZE) {
		fprintf (stderr, "memsuper_t not cache size: %zd\n",
				 sizeof (memline_t));
		return 1;
	}
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
				 super->page_size, sysconf (_SC_PAGESIZE));
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
	if (!test_block (super)) {
		fprintf (stderr, "block tests failed\n");
	}
	if (super->memblocks) {
		fprintf (stderr, "super block list not null 2\n");
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
