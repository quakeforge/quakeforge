#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#ifdef HAVE_STRING_H
# include <string.h>
#endif
#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif
#include <stdlib.h>

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

struct {
	const char *path;
	int         expect_offset;
} ext_tests[] = {
	{"foo", 3},
	{"foo.a", 3},
	{"foo.a.b", 5},
	{".foo", 4},
	{"bar/foo", 7},
	{"bar/foo.a", 7},
	{"bar/foo.a.b", 9},
	{"bar/.foo", 8},
	{"bar.a/foo", 9},
};
#define num_ext_tests (sizeof (ext_tests) / sizeof (ext_tests[0]))

struct {
	const char *path;
	const char *expect;
} strip_tests[] = {
	{"foo", "foo"},
	{"foo.a", "foo"},
	{"foo.a.b", "foo.a"},
	{".foo", ".foo"},
	{"bar/foo", "bar/foo"},
	{"bar/foo.a", "bar/foo"},
	{"bar/foo.a.b", "bar/foo.a"},
	{"bar/.foo", "bar/.foo"},
	{"bar.a/foo", "bar.a/foo"},
};
#define num_strip_tests (sizeof (strip_tests) / sizeof (strip_tests[0]))

int
main (int argc, const char **argv)
{
	size_t      i;
	int         res = 0;

	for (i = 0; i < num_path_tests; i++) {
		const char *cpath = QFS_CompressPath (path_tests[i].path);
		if (strcmp (cpath, path_tests[i].expect)) {
			fprintf (stderr, "FAIL: (%zd) \"%s\" -> \"%s\", got \"%s\"\n", i,
					 path_tests[i].path, path_tests[i].expect, cpath);
			res = 1;
		}
	}

	for (i = 0; i < num_ext_tests; i++) {
		const char *ext = QFS_FileExtension (ext_tests[i].path);
		if (ext - ext_tests[i].path != ext_tests[i].expect_offset) {
			fprintf (stderr, "FAIL: (%zd) \"%s\" -> %d (%s), got %d (%s)\n", i,
					 ext_tests[i].path, ext_tests[i].expect_offset,
					 ext_tests[i].path + ext_tests[i].expect_offset,
					 (int) (ext - ext_tests[i].path), ext);
			res = 1;
		}
	}

	for (i = 0; i < num_strip_tests; i++) {
		char       *strip = strdup (strip_tests[i].path);
		QFS_StripExtension (strip, strip);
		if (strcmp (strip, strip_tests[i].expect)) {
			fprintf (stderr, "FAIL: (%zd) \"%s\" -> \"%s\", got \"%s\"\n", i,
					 strip_tests[i].path, strip_tests[i].expect, strip);
			res = 1;
		}
		free (strip);
	}
	return res;
}
