#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <threads.h>

#include "QF/dstring.h"
#include "QF/mathlib.h"
#include "QF/ringbuffer.h"
#include "QF/sys.h"

typedef int (*test_func) (int a, int b);

typedef RING_BUFFER (int, 8) ringbuffer_t;
typedef RING_BUFFER (char, 8) ringbuffer_char_t;
typedef RING_BUFFER_ATOMIC (char, 8) ringbuffer_atomic_t;

static ringbuffer_t int_ringbuffer = { .buffer = { [0 ... 7] = 0xdeadbeef } };
static ringbuffer_char_t ringbuffer_char;
static ringbuffer_atomic_t ringbuffer_atomic;
mtx_t rb_mtx;
cnd_t rb_write_cnd;
cnd_t rb_read_cnd;

ringbuffer_atomic_t int_ringbuffer_atomic;

static int
get_size (int a, int b)
{
	return RB_buffer_size (&int_ringbuffer);
}

static int
space_available (int a, int b)
{
	return RB_SPACE_AVAILABLE (int_ringbuffer);
}

static int
data_available (int a, int b)
{
	return RB_DATA_AVAILABLE (int_ringbuffer);
}

static int
write_data (int value, int b)
{
	return RB_WRITE_DATA (int_ringbuffer, &value, 1);
}

static int
acquire (int count, int b)
{
	return *RB_ACQUIRE (int_ringbuffer, count);
}

static int
read_data (int a, int b)
{
	int         ret;
	RB_READ_DATA (int_ringbuffer, &ret, 1);
	return ret;
}

static int
release (int count, int b)
{
	return RB_RELEASE (int_ringbuffer, count);
}

static int
peek_data (int offset, int b)
{
	return *RB_PEEK_DATA (int_ringbuffer, offset);
}

static int
poke_data (int offset, int value)
{
	return RB_POKE_DATA (int_ringbuffer, offset, value);
}

static int
init_threading (int a, int b)
{
	if (mtx_init (&rb_mtx, mtx_plain) != thrd_success) {
		return -1;
	}
	if (cnd_init (&rb_write_cnd) != thrd_success) {
		return -1;
	}
	if (cnd_init (&rb_read_cnd) != thrd_success) {
		return -1;
	}
	return 0;
}

static const char *text = "Sometimes, it is necessary to write long gibberish "
"in order to test things. Hopefully, this may be an exception, though it's "
"not exactly the most useful or intersting text around.";

static dstring_t *out_text;

static int
compare_strings (int a, int b)
{
	int         ret = strcmp (out_text->str, text);
	dstring_delete (out_text);
	out_text = 0;
	return ret;
}

static int
locked_reader (void *data)
{
	if (!out_text) {
		out_text = dstring_new ();
	}
	dstring_clear (out_text);

	do {
		mtx_lock (&rb_mtx);
		while (RB_DATA_AVAILABLE (ringbuffer_char) < 1) {
			cnd_wait (&rb_read_cnd, &rb_mtx);
		}
		unsigned    len = RB_DATA_AVAILABLE (ringbuffer_char);
		RB_READ_DATA (ringbuffer_char, dstring_reserve (out_text, len), len);
		cnd_signal (&rb_write_cnd);
		mtx_unlock (&rb_mtx);
	} while (out_text->str[out_text->size - 1]);

	return 0;
}

static int
locked_writer (void *data)
{
	size_t      data_len = strlen (text) + 1;	// send nul terminator too
	const char *str = text;

	while (data_len > 0) {
		mtx_lock (&rb_mtx);
		while (RB_SPACE_AVAILABLE (ringbuffer_char) < 1) {
			cnd_wait (&rb_write_cnd, &rb_mtx);
		}
		unsigned    len = RB_SPACE_AVAILABLE (ringbuffer_char);
		len = min (len, data_len);
		RB_WRITE_DATA (ringbuffer_char, str, len);
		str += len;
		data_len -= len;
		cnd_signal (&rb_read_cnd);
		mtx_unlock (&rb_mtx);
	}

	return 0;
}

static int
run_locked (int a, int b)
{
	thrd_t      writer;
	thrd_t      reader;

	if (thrd_create (&writer, locked_writer, 0) != thrd_success) {
		return -1;
	}
	if (thrd_create (&reader, locked_reader, 0) != thrd_success) {
		return -1;
	}

	int         res;
	if (thrd_join (writer, &res) != thrd_success) {
		return -1;
	}
	if (thrd_join (reader, &res) != thrd_success) {
		return -1;
	}
	return 0;
}

static int
free_reader (void *data)
{
	if (!out_text) {
		out_text = dstring_new ();
	}
	dstring_clear (out_text);

	do {
		while (RB_DATA_AVAILABLE (ringbuffer_atomic) < 1) {
		}
		unsigned    len = RB_DATA_AVAILABLE (ringbuffer_atomic);
		RB_READ_DATA (ringbuffer_atomic, dstring_reserve (out_text, len), len);
	} while (out_text->str[out_text->size - 1]);

	return 0;
}

static int
free_writer (void *data)
{
	size_t      data_len = strlen (text) + 1;	// send nul terminator too
	const char *str = text;

	while (data_len > 0) {
		while (RB_SPACE_AVAILABLE (ringbuffer_atomic) < 1) {
		}
		unsigned    len = RB_SPACE_AVAILABLE (ringbuffer_atomic);
		len = min (len, data_len);
		RB_WRITE_DATA (ringbuffer_atomic, str, len);
		str += len;
		data_len -= len;
	}

	return 0;
}

static int
run_free (int a, int b)
{
	thrd_t      writer;
	thrd_t      reader;

	if (thrd_create (&writer, free_writer, 0) != thrd_success) {
		return -1;
	}
	if (thrd_create (&reader, free_reader, 0) != thrd_success) {
		return -1;
	}

	int         res;
	if (thrd_join (writer, &res) != thrd_success) {
		return -1;
	}
	if (thrd_join (reader, &res) != thrd_success) {
		return -1;
	}
	return 0;
}

#define _ 0	// unused parameter

struct {
	test_func   test;
	int         param1, param2;
	int         test_expect;
} tests[] = {
	{ get_size,        _, _, 8 },
	{ space_available, _, _, 7 },
	{ data_available,  _, _, 0 },
	{ write_data,      0x600dc0de, _, 1 },
	{ space_available, _, _, 6 },
	{ data_available,  _, _, 1 },
	{ acquire,         0, _, 0xdeadbeef },	// iffy test: 0 count
	{ space_available, _, _, 6 },
	{ data_available,  _, _, 1 },
	{ read_data,       1, _, 0x600dc0de },
	{ space_available, _, _, 7 },
	{ data_available,  _, _, 0 },
	{ release,         0, _, 1 },
	{ space_available, _, _, 7 },
	{ data_available,  _, _, 0 },
	{ poke_data,       0, 1, 1 },
	{ poke_data,       1, 2, 2 },
	{ poke_data,       2, 3, 3 },
	{ poke_data,       3, 4, 4 },
	{ poke_data,       4, 5, 5 },
	{ poke_data,       5, 6, 6 },
	{ acquire,         6, _, 1 },
	{ space_available, _, _, 1 },
	{ data_available,  _, _, 6 },
	{ write_data,      7, _, 0 },	// head wrapped to 0
	{ space_available, _, _, 0 },
	{ data_available,  _, _, 7 },
	{ peek_data,       0, _, 1 },
	{ peek_data,       1, _, 2 },
	{ peek_data,       2, _, 3 },
	{ peek_data,       3, _, 4 },
	{ peek_data,       4, _, 5 },
	{ peek_data,       5, _, 6 },
	{ peek_data,       6, _, 7 },
	{ space_available, _, _, 0 },
	{ data_available,  _, _, 7 },
	{ init_threading,  _, _, 0 },
	{ run_locked,      _, _, 0 },
	{ compare_strings, _, _, 0 },
	{ run_free,      _, _, 0 },
	{ compare_strings, _, _, 0 },
};
#define num_tests (sizeof (tests) / sizeof (tests[0]))
int test_start_line = __LINE__ - num_tests - 2;

int
main (int argc, const char **argv)
{
	int         res = 0;

	for (size_t i = 0; i < num_tests; i++) {
		int         test_res;
		test_res = tests[i].test (tests[i].param1, tests[i].param2);
		if (test_res != tests[i].test_expect) {
			res |= 1;
			printf ("test %zd (line %zd) failed\n", i, i + test_start_line);
			printf ("expect: %d\n", tests[i].test_expect);
			printf ("got   : %d\n", test_res);
		}
	}

	return res;
}
