struct plitem_s {integer dummy;};
typedef struct plitem_s plitem_t;

typedef struct foo_s {
	integer x;
	plitem_t i;
} foo_t;

foo_t snafu (foo_t x) = #0;
foo_t zap (void) = #0;

foo_t aa[5];
foo_t x;

foo_t bar (foo_t *foo, plitem_t item)
{
//	x = aa[foo.x];
//	aa[foo.x + 1] = aa[foo.x - 1];
//	aa[foo.x] = x;
//	snafu (aa[foo.x]);
//	aa[foo.x] = snafu (aa[foo.x]);
	aa[foo.x] = zap ();
//	foo.i = item;
//	return aa[foo.x];
	return zap ();
}
