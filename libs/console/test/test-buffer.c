#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>

#include "QF/console.h"

static int
test_1 (void)
{
	int         ret = 1;
	__auto_type con = Con_CreateBuffer (1024, 25);
	if (!con) {
		printf ("con_buffer allocation failed\n");
		goto fail;
	}
	if (!con->buffer) {
		printf ("con_buffer buffer not set\n");
		goto fail;
	}
	if (!con->lines) {
		printf ("con_buffer lines not set\n");
		goto fail;
	}
	if (con->buffer_size != 1024) {
		printf ("con_buffer buffer_size incorrect: %zd\n", con->buffer_size);
		goto fail;
	}
	if (con->max_lines != 25) {
		printf ("con_buffer max_lines incorrect: %d\n", con->max_lines);
		goto fail;
	}
	if (con->num_lines != 1) {
		printf ("con_buffer num_lines incorrect: %d\n", con->num_lines);
		goto fail;
	}
	if (con->cur_line != 0) {
		printf ("con_buffer cur_line incorrect: %d\n", con->cur_line);
		goto fail;
	}
	if (con->lines[con->cur_line].text != con->buffer) {
		printf ("con_buffer cur_line.text incorrect: %p, %p\n",
				con->lines[con->cur_line].text, con->buffer);
		goto fail;
	}
	if (con->lines[con->cur_line].len != 0) {
		printf ("con_buffer cur_line.line incorrect: %zd\n",
				con->lines[con->cur_line].len);
		goto fail;
	}

	ret = 0;
fail:
	Con_DestroyBuffer (con);
	return ret;
}

static int
test_2 (void)
{
	int         ret = 1;
	char        text[2049];

	for (size_t i = 0; i < sizeof (text); i++) {
		int         x = i % 13;
		text[i] = x + (x > 9 ? 'a' - 10 : '0');
	}
	text[sizeof(text) - 1] = 0;

	__auto_type con = Con_CreateBuffer (1024, 25);
	Con_BufferAddText (con, text);

	if (con->num_lines != 1) {
		printf ("con_buffer num_lines incorrect: %d\n", con->num_lines);
		goto fail;
	}
	if (con->cur_line != 0) {
		printf ("con_buffer cur_line incorrect: %d\n", con->cur_line);
		goto fail;
	}
	if (con->lines[con->cur_line].text != con->buffer) {
		printf ("con_buffer cur_line.text incorrect: %p, %p\n",
				con->lines[con->cur_line].text, con->buffer);
		goto fail;
	}
	if (con->lines[con->cur_line].len != 1024) {
		printf ("con_buffer cur_line.line incorrect: %zd\n",
				con->lines[con->cur_line].len);
		goto fail;
	}
	if (memcmp (con->buffer, text + 1024, 1024)) {
		printf ("con_buffer incorrect\n");
		goto fail;
	}

	ret = 0;
fail:
	if (ret) {
		printf ("%s failed\n", __FUNCTION__);
	}
	Con_DestroyBuffer (con);
	return ret;
}

int
main (void)
{
	int         ret = 0;
	ret |= test_1 ();
	ret |= test_2 ();
	return ret;
}
