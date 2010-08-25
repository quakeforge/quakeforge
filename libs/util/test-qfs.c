#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "QF/quakefs.h"

struct {
	const char *path;
	const char *expect;
} path_tests [] = {
	{"", ""},
	{"/", "/"},
	{"\\", "/"},
	{".", ""},
	{"./", ""},
	{"/.", "/"},
	{"..", ".."},
	{"/..", "/"},
	{"foo/..", ""},
	{"foo/bar/..", "foo"},
	{"foo//bar", "foo/bar"},
	{"../foo/..", ".."},
	{"\\blah\\../foo/..\\baz/.\\x", "/baz/x"},
};
#define num_path_tests (sizeof (path_tests) / sizeof (path_tests[0]))

int
main (int argc, const char **argv)
{
	size_t      i;
	int         res = 0;

	for (i = 0; i < num_path_tests; i++) {
		char       *cpath = QFS_CompressPath (path_tests[i].path);
		if (strcmp (cpath, path_tests[i].expect)) {
			fprintf (stderr, "FAIL: (%zd) \"%s\" -> \"%s\", got \"%s\"\n", i,
					 path_tests[i].path, path_tests[i].expect, cpath);
			res = 1;
		}
	}
	return res;
}
