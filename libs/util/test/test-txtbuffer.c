#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>

#include "QF/txtbuffer.h"

static size_t
check_text_ptr (txtbuffer_t *buffer, size_t offset, const char *txt,
				size_t length)
{
	return buffer->text != 0;
}

static size_t
get_textSize (txtbuffer_t *buffer, size_t offset, const char *txt,
				size_t length)
{
	return buffer->textSize;
}

static size_t
get_bufferSize (txtbuffer_t *buffer, size_t offset, const char *txt,
				size_t length)
{
	return buffer->bufferSize;
}

static size_t
get_gapOffset (txtbuffer_t *buffer, size_t offset, const char *txt,
				size_t length)
{
	return buffer->gapOffset;
}

static size_t
get_gapSize (txtbuffer_t *buffer, size_t offset, const char *txt,
				size_t length)
{
	return buffer->gapSize;
}

static size_t
insert_text (txtbuffer_t *buffer, size_t offset, const char *txt,
				size_t length)
{
	if (TextBuffer_InsertAt (buffer, offset, txt, length)) {
		memset (buffer->text + buffer->gapOffset, 0xff, buffer->gapSize);
		return 1;
	}
	return 0;
}

static size_t
delete_text (txtbuffer_t *buffer, size_t offset, const char *txt,
				size_t length)
{
	if (TextBuffer_DeleteAt (buffer, offset, length)) {
		memset (buffer->text + buffer->gapOffset, 0xff, buffer->gapSize);
		return 1;
	}
	return 0;
}

static size_t
destroy_buffer (txtbuffer_t *buffer, size_t offset, const char *txt,
				size_t length)
{
	TextBuffer_Destroy (buffer);
	return 0;
}

typedef size_t (*test_func) (txtbuffer_t *buffer, size_t offset,
							 const char *text, size_t length);

static const char test_text[] = {
	"Guarding the entrance to the Grendal\n"
	"Gorge is the Shadow Gate, a small keep\n"
	"and monastary which was once the home\n"
	"of the Shadow cult.\n\n"
	"For years the Shadow Gate existed in\n"
	"obscurity but after the cult discovered\n"
	"the \3023\354\341\343\353\240\307\341\364\345 in the caves below\n"
	"the empire took notice.\n"
	"A batallion of Imperial Knights were\n"
	"sent to the gate to destroy the cult\n"
	"and claim the artifact for the King.",
};
static const char empty[TXTBUFFER_GROW] = { };

static size_t
compare_text (txtbuffer_t *buffer, size_t offset, const char *txt,
				size_t length)
{
	//printf("%zd %ld %zd\n", offset, txt - test_text, length);
	size_t res = memcmp (buffer->text + offset, txt, length) == 0;
	if (!res) {
		for (size_t i = 0; i < length; i++) {
			//printf ("%02x %02x\n", txt[i] & 0xff, buffer->text[offset + i] & 0xff);
		}
	}
	return res;
}

#define txtsize (sizeof (test_text) - 1)
#define txtsize0 txtsize
#define bufsize0 (TXTBUFFER_GROW)
#define gapoffs0 txtsize0
#define gapsize0 (bufsize0 - txtsize0)
#define subtxtlen 200
#define suboffs 200
#define txtsize1 (txtsize0 + subtxtlen)
#define bufsize1 (TXTBUFFER_GROW)
#define gapoffs1 (suboffs + subtxtlen)
#define gapsize1 (bufsize1 - txtsize1)
#define deltxtlen 350
#define deloffs 150
#define txtsize2 (txtsize + subtxtlen - deltxtlen)
#define bufsize2 (TXTBUFFER_GROW)
#define gapoffs2 (deloffs)
#define gapsize2 (bufsize2 - txtsize2)
#define deltxtrem (txtsize2 - gapoffs2)
#define emptyoffs 370
#define txtsize3 (txtsize + sizeof (empty))
#define bufsize3 (2 * TXTBUFFER_GROW)
#define gapoffs3 (emptyoffs + sizeof (empty))
#define gapsize3 (bufsize3 - txtsize3)
#define txtsize4 (txtsize + 2 * sizeof (empty))
#define bufsize4 (3 * TXTBUFFER_GROW)
#define gapoffs4 (emptyoffs + sizeof (empty))
#define gapsize4 (bufsize4 - txtsize4)

struct {
	test_func   test;
	size_t      offset_param;
	const char *text_param;
	size_t      length_param;
	int         test_expect;
} tests[] = {
	{ check_text_ptr, 0, 0, 0, 0 },
	{ get_textSize,   0, 0, 0, 0 },
	{ get_bufferSize, 0, 0, 0, 0 },
	{ get_gapOffset,  0, 0, 0, 0 },
	{ get_gapSize,    0, 0, 0, 0 },
	{ insert_text,    0, test_text, txtsize, 1 },
	{ get_textSize,   0, 0, 0, txtsize0 },
	{ get_bufferSize, 0, 0, 0, bufsize0 },
	{ get_gapOffset,  0, 0, 0, gapoffs0 },
	{ get_gapSize,    0, 0, 0, gapsize0 },
	{ compare_text,   0, test_text, txtsize, 1 },
	{ insert_text,    suboffs, test_text, subtxtlen, 1 },
	{ get_textSize,   0, 0, 0, txtsize1 },
	{ get_bufferSize, 0, 0, 0, bufsize1 },
	{ get_gapOffset,  0, 0, 0, gapoffs1 },
	{ get_gapSize,    0, 0, 0, gapsize1 },
	{ compare_text,   0, test_text, txtsize, 0 },
	{ compare_text,   0, test_text, suboffs, 1 },
	{ compare_text,   suboffs, test_text, subtxtlen, 1 },
	{ compare_text,   gapoffs1 + gapsize1, test_text + subtxtlen, txtsize - subtxtlen, 1 },
	{ delete_text,    deloffs, 0, deltxtlen, 1 },
	{ get_textSize,   0, 0, 0, txtsize2 },
	{ get_bufferSize, 0, 0, 0, bufsize2 },
	{ get_gapOffset,  0, 0, 0, gapoffs2 },
	{ get_gapSize,    0, 0, 0, gapsize2 },
	{ compare_text,   0, test_text, suboffs, 0 },
	{ compare_text,   suboffs, test_text, subtxtlen, 0 },
	{ compare_text,   gapoffs1 + gapsize1, test_text + subtxtlen, txtsize - subtxtlen, 0 },
	{ compare_text,   0, test_text, deloffs, 1 },
	{ compare_text,   gapoffs2 + gapsize2, test_text + txtsize - deltxtrem, deltxtrem, 1 },
	{ compare_text,   gapoffs2 + gapsize2 - 1, test_text + txtsize - deltxtrem - 1, 1, 0 },
	{ delete_text,    0, 0, -1, 1 },
	{ check_text_ptr, 0, 0, 0, 1 },
	{ get_textSize,   0, 0, 0, 0 },
	{ get_bufferSize, 0, 0, 0, bufsize2 },
	{ get_gapOffset,  0, 0, 0, 0 },
	{ get_gapSize,    0, 0, 0, bufsize2 },
	{ insert_text,    1, test_text, txtsize, 0 },
	{ get_textSize,   0, 0, 0, 0 },
	{ get_bufferSize, 0, 0, 0, bufsize2 },
	{ get_gapOffset,  0, 0, 0, 0 },
	{ get_gapSize,    0, 0, 0, bufsize2 },
	{ insert_text,    0, test_text, txtsize, 1 },
	{ insert_text,    300, 0, 0, 1 },
	{ get_textSize,   0, 0, 0, txtsize0 },
	{ get_bufferSize, 0, 0, 0, bufsize2 },
	{ get_gapOffset,  0, 0, 0, 300 },
	{ get_gapSize,    0, 0, 0, gapsize0 },
	{ compare_text,   0, test_text, 300, 1 },
	{ compare_text,   300, test_text + 300, 1, 0 },
	{ compare_text,   300 + gapsize0, test_text + 300, txtsize - 300, 1 },
	{ compare_text,   300 + gapsize0 - 1, test_text + 300 - 1, 1, 0 },
	{ insert_text,    350, 0, 0, 1 },
	{ compare_text,   (emptyoffs - 20) + gapsize0, test_text + (emptyoffs - 20), txtsize - (emptyoffs - 20), 1 },
	{ get_textSize,   0, 0, 0, txtsize0 },
	{ get_bufferSize, 0, 0, 0, bufsize2 },
	{ get_gapOffset,  0, 0, 0, (emptyoffs - 20) },
	{ get_gapSize,    0, 0, 0, gapsize0 },
	{ insert_text,    emptyoffs, empty, sizeof (empty), 1 },
	{ get_textSize,   0, 0, 0, txtsize3 },
	{ get_bufferSize, 0, 0, 0, bufsize3 },
	{ get_gapOffset,  0, 0, 0, gapoffs3 },
	{ get_gapSize,    0, 0, 0, gapsize3 },
	{ insert_text,    emptyoffs, empty, sizeof (empty), 1 },
	{ get_textSize,   0, 0, 0, txtsize4 },
	{ get_bufferSize, 0, 0, 0, bufsize4 },
	{ get_gapOffset,  0, 0, 0, gapoffs4 },
	{ get_gapSize,    0, 0, 0, gapsize4 },
	{ destroy_buffer, 0, 0, 0, 0 },
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))
int test_start_line = __LINE__ - num_tests - 2;

int
main (int argc, const char **argv)
{
	size_t      i;
	int         res = 0;

	txtbuffer_t *buffer = TextBuffer_Create ();

	for (i = 0; i < num_tests; i++) {
		if (tests[i].test) {
			int         test_res;
			test_res = tests[i].test (buffer,
									  tests[i].offset_param,
									  tests[i].text_param,
									  tests[i].length_param);
			if (test_res != tests[i].test_expect) {
				res |= 1;
				printf ("test %d (line %d) failed\n", (int) i,
						(int) i + test_start_line);
				printf ("expect: %d\n", tests[i].test_expect);
				printf ("got   : %d\n", test_res);
				continue;
			}
		}
	}
	return res;
}
