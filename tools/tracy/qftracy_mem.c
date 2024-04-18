#ifdef TRACY_ENABLE

#include <string.h>

void *qf_aligned_alloc (size_t a, size_t s) asm("aligned_alloc");
void *qf_malloc (size_t s) asm("malloc");
void *qf_calloc (size_t c, size_t s) asm("calloc");
void *qf_realloc (void *p, size_t s) asm("realloc");
void qf_free (void *p) asm("free");
char *qf_strdup (const char *s) asm("strdup");
char *qf_getcwd (char *b, size_t s) asm("getcwd");

void *aligned_alloc (size_t a, size_t s)
{
	void *p = qf_aligned_alloc (a, s);
	if (p) {
		TracyCAlloc (p, s);
	}
	return p;
}

void *malloc (size_t s)
{
	void *p = qf_malloc (s);
	if (p) {
		TracyCAlloc (p, s);
	}
	return p;
}

void *calloc (size_t c, size_t s)
{
	void *p = qf_calloc (c, s);
	if (p) {
		TracyCAlloc (p, c * s);
	}
	return p;
}

void *realloc (void *p, size_t s)
{
	if (p) {
		TracyCFree(p);
	}
	p = qf_realloc (p, s);
	if (p) {
		TracyCAlloc (p, s);
	}
	return p;
}

void free (void *p)
{
	if (p) {
		TracyCFree(p);
	}
	qf_free (p);
}

char *strdup (const char *s)
{
	char *p = qf_strdup (s);
	if (p) {
		TracyCAlloc (p, strlen (p) + 1);
	}
	return p;
}

char *getcwd (char *b, size_t s)
{
	if (!b) {
		char *p = qf_getcwd (b, s);
		if (p) {
			TracyCAlloc (p, strlen (p) + 1);
		}
		return p;
	} else {
		return qf_getcwd (b, s);
	}
}
#endif
