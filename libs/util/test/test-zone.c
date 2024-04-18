#define PARANOID
#include "../zone.c"

static int
check_hunk_block (const memhunk_t *hunk, const void *mem, size_t size)
{
	const hunkblk_t  *h = (const hunkblk_t *) mem - 1;
	if (h->sentinel1 != HUNK_SENTINEL || h->sentinel2 != HUNK_SENTINEL) {
		printf ("invalid sentinels: %u %u\n", h->sentinel1, h->sentinel2);
		return 1;
	}
	if (h->size % HUNK_ALIGN) {
		printf ("block size misaligned: %zd %zd\n",
				h->size, h->size % HUNK_ALIGN);
		return 1;
	}
	if (h->size - sizeof (hunkblk_t) < size) {
		printf ("block size too small: %zd %zd\n",
				h->size - sizeof (hunkblk_t), size);
		return 1;
	}
	if (h->size - sizeof (hunkblk_t) - size >= HUNK_ALIGN) {
		printf ("block size too small: %zd %d\n",
				h->size - sizeof (hunkblk_t) - size, HUNK_ALIGN);
		return 1;
	}
	if ((byte *) h < hunk->base
		|| (byte *) h + h->size > hunk->base + hunk->size) {
		printf ("block outside of hunk: %p %p, %p %p\n",
				h, hunk->base, (byte *) h + h->size, hunk->base + hunk->size);
		return 1;
	}
	size_t      offset = (byte *) h - hunk->base;
	if (offset < hunk->size - hunk->high_used
		&& offset + h->size > hunk->low_used) {
		printf ("block in unallocated region: %zd %zd, %zd %zd\n",
				offset, offset + h->size,
				hunk->low_used, hunk->size - hunk->high_used);
		return 1;
	}
	return 0;
}

static int
check_cache_block (const memhunk_t *hunk, const void *c, size_t size)
{
	if (!c) {
		printf ("cache block is null\n");
		return 1;
	}

	const cache_system_t *cs = (cache_system_t *) c - 1;

	size_t      hunk_high = hunk->size - hunk->high_used;
	size_t      offset = (byte *) cs - hunk->base;
	if (offset < hunk->low_used || offset + cs->size > hunk_high) {
		printf ("cache block in hunk stack: %zd %zd, %zd, %zd\n",
				offset, offset + cs->size, hunk->low_used, hunk_high);
		return 1;
	}
	return 0;
}

static int
check_cache_order (memhunk_t *hunk)
{
	uint32_t    ci;
	cache_system_t *c;
	int         ret = 0;

	cache_system_t *first = cs_ptr (hunk, hunk->cache_head[0].next);
	cache_system_t *last = cs_ptr (hunk, hunk->cache_head[0].prev);

	if (first->prev) {
		printf ("first cache block does not point back to head: %u\n",
				first->prev);
	}
	if (last->next) {
		printf ("last cache block does not point forward to head: %u\n",
				last->next);
	}
	if (first->prev || last->next) {
		exit (1);	// unsafe to continue
	}
	uint32_t    prev = 0;
	for (ci = hunk->cache_head[0].next; ci; prev = ci, ci = c->next) {
		c = cs_ptr (hunk, ci);
		if ((byte *) c + c->size > hunk->base + hunk->size) {
			printf ("cache block outside of hunk (from %u)\n", prev);
			exit (1);
		}
		if (c->readlock) {
			printf ("look in cache detected at %u\n", ci);
			exit (1);
		}
		c->readlock = 1;
		if (c->next && c->next < ci) {
			printf ("cache block sequence incorrect %u -> %u\n", prev, ci);
			ret |= 1;
		}
	}
	return ret;
}

int
main (int argc, const char **argv)
{
	int         ret = 0;

	if (sizeof (memhunk_t) != 128) {
		ret = 1;
		printf ("memhunk_t not 128 bytes: %zd\n", sizeof (memhunk_t));
	}
	if (sizeof (hunkblk_t) != 64) {
		ret = 1;
		printf ("hunkblk_t not 64 bytes: %zd\n", sizeof (memhunk_t));
	}
	size_t      memsize = 64 * 1024;
	void       *hunk_mem = Sys_Alloc (memsize);
	if ((intptr_t) hunk_mem & 63) {
		// (not really part of test, but relied upon) should be 4k
		// aligned, but doesn't matter for the tests
		printf ("Sys_Alloc returned unaligned memory");
		ret = 1;
	}
	memhunk_t  *hunk = Hunk_Init (hunk_mem, memsize);
	if ((void *) hunk != hunk_mem) {
		ret = 1;
		printf ("hunk moved\n");
	}
	if ((void *) hunk->base != (void *)&hunk[1]) {
		ret = 1;
		printf ("hunk->base does not point to beginning of hunk space\n");
	}
	if (hunk->size != memsize - sizeof (memhunk_t)) {
		ret = 1;
		printf ("hunk size not memsize - sizeof (memhunk_t) (%zd - %zd): %zd\n",
				memsize, sizeof (memhunk_t), hunk->size);
	}
	if (hunk->low_used || hunk->high_used) {
		ret = 1;
		printf ("hunk low and high used not 0: %zd %zd\n",
				hunk->low_used, hunk->high_used);
	}
	if (hunk->tempmark || hunk->tempactive) {
		ret = 1;
		printf ("hunk tempmark tempactive not 0: %zd %d\n",
				hunk->tempmark, hunk->tempactive);
	}
	if (ret) {
		// no point continuing
		return 1;
	}

	size_t      low_mark = Hunk_LowMark (hunk);
	if (low_mark != 0) {
		ret = 1;
		printf ("initial hunk low mark not 0: %zd\n", low_mark);
	}
	size_t      size = 1024;
	void       *low = Hunk_AllocName (hunk, size, "low test");
	ret |= check_hunk_block (hunk, low, size);
	if (low > (void *) (hunk->base + sizeof (hunkblk_t))) {
		ret = 1;
		printf ("low memory not at beginning of hunk: %p %p\n",
				low, hunk->base + sizeof (hunkblk_t));
	}

	size_t      high_mark = Hunk_HighMark (hunk);
	if (high_mark != 0) {
		ret = 1;
		printf ("initial hunk high mark not 0: %zd\n", high_mark);
	}
	void       *high = Hunk_HighAlloc (hunk, size);//FIXME, "high test");
	ret |= check_hunk_block (hunk, high, size);
	if (high + size < (void *) (hunk->base + hunk->size)) {
		ret = 1;
		printf ("high memory not at end of hunk: %p %p\n",
				high + size, hunk->base + hunk->size);
	}

	global_hunk = hunk;//FIXME put hunk in cache_user_t ?
	cache_user_t cu = {};
	void       *cm = Cache_Alloc (&cu, 512, "cache test");
	ret |= check_cache_block (hunk, cm, 512);

	size_t      mark = Hunk_LowMark (hunk);
	void       *low2 = Hunk_AllocName (hunk, 61 * 1024, "low test 2");
	ret |= check_hunk_block (hunk, low2, 61 * 1024);
	if (cm == Cache_Check (&cu)) {
		ret = 1;
		printf ("cache block was not moved\n");
	}
	cm = Cache_Check (&cu);
	ret |= check_cache_block (hunk, cm, 512);
	Hunk_FreeToLowMark (hunk, mark);
	if (cm != Cache_Check (&cu)) {
		ret = 1;
		printf ("cache block was moved\n");
	}

	void       *high2 = Hunk_HighAlloc (hunk, 2 * 1024);
	ret |= check_hunk_block (hunk, high2, 2 * 1024);
	if (cm == Cache_Check (&cu)) {
		ret = 1;
		printf ("cache block was not moved\n");
	}
	cm = Cache_Check (&cu);
	ret |= check_cache_block (hunk, cm, 512);
	printf ("%zd %zd %zd\n", hunk->low_used, hunk->size - hunk->high_used,
			(byte *) cm - hunk->base);
	check_cache_order (hunk);
	Hunk_Print(hunk, 1);
	Cache_Print ();

	return ret;
}
