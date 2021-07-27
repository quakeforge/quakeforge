#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>

#include "QF/msg.h"
#include "QF/sizebuf.h"

static byte buffer[1024];
static sizebuf_t sb = { .data = buffer, .maxsize = sizeof (buffer) };
static qmsg_t msg = { .message = &sb, };

typedef struct {
	int32_t     input;
	unsigned    bytes;
} utf8_test_t;

static utf8_test_t tests[] = {
	{0x43214321, 6},
	{0x55555555, 6},
	{0x2aaaaaaa, 6},
	{0x55aa55aa, 6},
	{0x1df001ed, 6},	// hey, why not? good bit pattern
	{0x089abcde, 6},
	{0x049abcde, 6},
	{0x023abcde, 5},
	{0x012abcde, 5},
	{0x008abcde, 5},
	{0x004abcde, 5},
	{0x002abcde, 5},
	{0x001abcde, 4},
	{0x000abcde, 4},
	{0x0006bcde, 4},
	{0x0002bcde, 4},
	{0x0001bcde, 4},
	{0x0000bcde, 3},
	{0x00007cde, 3},
	{0x00003cde, 3},
	{0x00001cde, 3},
	{0x00000cde, 3},
	{0x000004de, 2},
	{0x000002de, 2},
	{0x000001de, 2},
	{0x000000de, 2},
	{0x0000005e, 1},
	{0x0000004e, 1},
	{0x0000002e, 1},
	{0x0000001e, 1},
	{0x0000000e, 1},
	{0x00000006, 1},
	{0x00000000, 1},
	{0x40000000, 6},
	{0x20000000, 6},
	{0x10000000, 6},
	{0x08000000, 6},
	{0x04000000, 6},
	{0x02000000, 5},
	{0x01000000, 5},
	{0x00800000, 5},
	{0x00400000, 5},
	{0x00200000, 5},
	{0x00100000, 4},
	{0x00080000, 4},
	{0x00040000, 4},
	{0x00020000, 4},
	{0x00010000, 4},
	{0x00008000, 3},
	{0x00004000, 3},
	{0x00002000, 3},
	{0x00001000, 3},
	{0x00000800, 3},
	{0x00000400, 2},
	{0x00000200, 2},
	{0x00000100, 2},
	{0x00000080, 2},
	{0x00000040, 1},
	{0x00000020, 1},
	{0x00000010, 1},
	{0x00000008, 1},
	{0x00000004, 1},
	{0x00000002, 1},
	{0x00000001, 1},
};
#define num_tests (sizeof (tests) / (sizeof (tests[0])))

int
main (int argc, const char **argv)
{
	int         res = 0;

	for (size_t i = 0; i < num_tests; i++) {
		sb.cursize = 0;
		msg.readcount = 0;
		msg.badread = 0;

		MSG_WriteUTF8 (&sb, tests[i].input);
		int32_t     output = MSG_ReadUTF8 (&msg);

		printf ("%d %08x\n", (int) i, tests[i].input);
		SZ_Dump (&sb);

		if (sb.cursize != tests[i].bytes || msg.readcount != tests[i].bytes
			|| output != tests[i].input || msg.badread) {
			res |= 1;
			printf ("test %d failed\n", (int) i);
			printf ("expect: %8x %d %d 0\n",
					tests[i].input, tests[i].bytes, tests[i].bytes);
			printf ("got   : %8x %d %d %d\n",
					output, sb.cursize, msg.readcount, msg.badread);
		}
	}
	return res;
}
