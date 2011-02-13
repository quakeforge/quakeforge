struct list_bucket_s {
	struct list_bucket_s *next;
	struct list_bucket_s **prev;
	id obj;
};
typedef struct list_bucket_s list_bucket_t;

id d (void)
{
	local list_bucket_t *e, *t = nil;
	return e.obj;
}
