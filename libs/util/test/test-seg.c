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
#include <stdio.h>

#include "QF/segtext.h"

static const char *test_string =
	"blah\n"
	"-- Vertex\n"
	"FOO\n"
	"-- Geometry\n"
	"BAR\n"
	"-- Fragment.Erosion\n"
	"BAZ\n"
	"-- Fragment.Grassfire\n"
	"QUX\n"
	"-- TessControl\n"
	"QUUX\n"
	"-- TessEvaluation\n"
	"QUUUX";

struct {
	const char *tag;
	const char *expect;
} seg_tests[] = {
	{"", 0},						// empty tags not findable
	{"Vertex", "FOO\n"},
	{"Geometry", "BAR\n"},
	{"Fragment.Erosion", "BAZ\n"},
	{"Fragment.Grassfire", "QUX\n"},
	{"TessControl", "QUUX\n"},
	{"TessEvaluation", "QUUUX"},	// final chunk doesn't have \n
	{"Vertex.Erosion", "FOO\n"},
	{"Geometry.Grasfire", "BAR\n"},
};
#define num_seg_tests (sizeof (seg_tests) / sizeof (seg_tests[0]))

struct {
	const char *tag;
	int         expect;
} line_tests[] = {
	{"Vertex", 3},
	{"Geometry", 5},
	{"Fragment.Erosion", 7},
	{"Fragment.Grassfire", 9},
	{"TessControl", 11},
	{"TessEvaluation", 13},	// final chunk doesn't have \n
};
#define num_line_tests (sizeof (line_tests) / sizeof (line_tests[0]))

int
main (int argc, const char **argv)
{
	size_t      i;
	int         res = 0;
	segtext_t  *st;

	st = Segtext_new (test_string);
	for (i = 0; i < num_seg_tests; i++) {
		const char *text = Segtext_Find (st, seg_tests[i].tag);
		if ((!text != !seg_tests[i].expect)
			&& (!text || !seg_tests[i].expect
				|| strcmp (text, seg_tests[i].expect))) {
			fprintf (stderr, "FAIL: (%zd) \"%s\" -> \"%s\", got \"%s\"\n", i,
					 seg_tests[i].tag, seg_tests[i].expect, text);
			res = 1;
		}
	}
	for (i = 0; i < num_line_tests; i++) {
		const segchunk_t *chunk = Segtext_FindChunk (st, line_tests[i].tag);
		if (!chunk || chunk->start_line != line_tests[i].expect) {
			fprintf (stderr, "FAIL: (%zd) \"%s\" -> %d, got %d\n", i,
					 line_tests[i].tag, line_tests[i].expect,
					 chunk->start_line);
			res = 1;
		}
	}
	return res;
}
