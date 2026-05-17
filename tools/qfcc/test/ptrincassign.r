#include "test-harness.h"

string str_list[5];

static string
str_something (string item)
{
	return item;
}

static void *
d_string_func (void *ptr, string item)
{
	auto p = (string *) ptr;
	*p++ = str_something (item);
	return p;
}

int
main ()
{
	auto p = &str_list[3];
	p = d_string_func (p, "foobar");
	bool ok = true;
	if (str_list[3] != "foobar") {
		printf ("str_list[3] != foobar: %s\n", str_list[3]);
		ok = false;
	}
	if (p != &str_list[4]) {
		printf ("p != &str_list[3]: %p %p\n", p, &str_list[4]);
		ok = false;
	}
	return ok ? 0 : 1;
}
