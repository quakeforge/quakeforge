struct plitem_s {integer dummy;};
typedef struct plitem_s plitem_t;

typedef struct foo_s {
	integer x;
	plitem_t i;
} foo_t;

void bar (foo_t *foo, plitem_t item)
{
	foo.i = item;
}
