#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <locale.h>

#include "QF/base64.h"
#include "QF/dstring.h"
#include "QF/mersenne.h"
#include "QF/sys.h"

typedef struct {
	const char *input;
	const char *expect;
} base64_test_t;

static base64_test_t encode_tests[] = {
	{"Many hands make light work.", "TWFueSBoYW5kcyBtYWtlIGxpZ2h0IHdvcmsu"},
	{"light work.", "bGlnaHQgd29yay4="},
	{"light work",  "bGlnaHQgd29yaw=="},
	{"light wor",   "bGlnaHQgd29y"},
	{"light wo",    "bGlnaHQgd28="},
	{"light w",     "bGlnaHQgdw=="},
};
#define num_encode_tests (sizeof (encode_tests) / (sizeof (encode_tests[0])))

int
main (int argc, const char **argv)
{
	int         res = 0;
	auto enc = dstring_new ();

	setlocale (LC_ALL, "");

	for (size_t i = 0; i < num_encode_tests; i++) {
		dstring_clear (enc);
		base64_encode (enc, encode_tests[i].input,
					   strlen (encode_tests[i].input));
		printf ("%d %s -> %s\n", (int) i, encode_tests[i].input, enc->str);

		if (strcmp (encode_tests[i].expect, enc->str) != 0) {
			res |= 1;
			printf ("test %d failed\n", (int) i);
			printf ("expect: %s\n", encode_tests[i].expect);
			printf ("got   : %s\n", enc->str);
		}
	}

	for (size_t i = 0; i < num_encode_tests; i++) {
		dstring_clear (enc);
		base64_decode (enc, encode_tests[i].expect,
					   strlen (encode_tests[i].expect));
		printf ("%d %s -> %s\n", (int) i, encode_tests[i].expect, enc->str);

		if (strcmp (encode_tests[i].input, enc->str) != 0) {
			res |= 1;
			printf ("test %d failed\n", (int) i);
			printf ("expect: %s\n", encode_tests[i].input);
			printf ("got   : %s\n", enc->str);
		}
	}

	size_t count = 1<<20;
	byte *data = malloc (count);
	{
		auto start = Sys_LongTime ();
		mtstate_t mt;
		mtwist_seed (&mt, 0xdeadbeef);

		for (size_t i = 0; i < count; i += 4) {
			uint32_t val = mtwist_rand (&mt);
			data[i + 0] = (val >>  0) & 0xff;
			data[i + 1] = (val >>  8) & 0xff;
			data[i + 2] = (val >> 16) & 0xff;
			data[i + 3] = (val >> 24) & 0xff;
		}
		auto end = Sys_LongTime ();
		printf ("generated %'zd bytes in %'zdus\n", count, end - start);
	}
	uint64_t total = 0;
	for (int i = 0; i < 10; i++) {
		dstring_clear (enc);
		dstring_reserve (enc, count * 2);
		auto start = Sys_LongTime ();
		base64_encode (enc, data, count);
		auto end = Sys_LongTime ();
		printf ("encoded %'zd bytes in %'zdus\n", count, end - start);
		total += end - start;
	}
	printf ("average for %'zd bytes: %'zdus\n", count, total / 10);
	total = 0;
	auto dec = dstring_new ();
	for (int i = 0; i < 10; i++) {
		dstring_clear (dec);
		dstring_reserve (dec, count);
		auto start = Sys_LongTime ();
		base64_encode (dec, enc->str, enc->size - 1);
		auto end = Sys_LongTime ();
		printf ("decoded %'zd bytes in %'zdus\n", count, end - start);
		total += end - start;
	}
	printf ("average for %'zd bytes: %'zdus\n", count, total / 10);
	free (data);
	dstring_delete (enc);
	dstring_delete (dec);
	return res;
}
