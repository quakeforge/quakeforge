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
		printf ("con_buffer buffer_size incorrect: %u\n", con->buffer_size);
		goto fail;
	}
	if (con->max_lines != 25) {
		printf ("con_buffer max_lines incorrect: %d\n", con->max_lines);
		goto fail;
	}
	if (con->line_head != 1) {
		printf ("con_buffer line_head incorrect: %d\n", con->line_head);
		goto fail;
	}
	if (con->line_tail != 0) {
		printf ("con_buffer line_tail incorrect: %d\n", con->line_tail);
		goto fail;
	}
	if (con->lines[con->line_tail].text != 0) {
		printf ("con_buffer line_tail.text incorrect: %u\n",
				con->lines[con->line_tail].text);
		goto fail;
	}
	if (con->lines[con->line_tail].len != 0) {
		printf ("con_buffer line_tail.len incorrect: %u\n",
				con->lines[con->line_tail].len);
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

	if (con->line_head != 1) {
		printf ("con_buffer line_head incorrect: %d\n", con->line_head);
		goto fail;
	}
	if (con->line_tail != 0) {
		printf ("con_buffer line_tail incorrect: %d\n", con->line_tail);
		goto fail;
	}
	if (con->lines[con->line_tail].text != 1) {
		printf ("con_buffer line_tail.text incorrect: %u\n",
				con->lines[con->line_tail].text);
		goto fail;
	}
	if (con->lines[con->line_tail].len != 1023) {
		printf ("con_buffer line_tail.len incorrect: %u\n",
				con->lines[con->line_tail].len);
		goto fail;
	}
	if (memcmp (con->buffer + 1, text + 1025, 1023)
		|| con->buffer[0] != 0) {
		printf ("con_buffer incorrect\n");
		goto fail;
	}

	// Add a single char at the end of the full buffer. The buffer should
	// just effectively scroll through the text as chars are added (via
	// the single line object maintaining constant length but updating its
	// text pointer)
	Con_BufferAddText (con, "N");

	if (con->line_head != 1) {
		printf ("2 con_buffer line_head incorrect: %d\n", con->line_head);
		goto fail;
	}
	if (con->line_tail != 0) {
		printf ("2 con_buffer line_tail incorrect: %d\n", con->line_tail);
		goto fail;
	}
	if (con->lines[con->line_tail].text != 2) {
		printf ("2 con_buffer line_tail.text incorrect: %u\n",
				con->lines[con->line_tail].text);
		goto fail;
	}
	if (con->lines[con->line_tail].len != 1023) {
		printf ("2 con_buffer line_tail.len incorrect: %u\n",
				con->lines[con->line_tail].len);
		goto fail;
	}
	if (memcmp (con->buffer + 2, text + 1026, 1022)
		|| con->buffer[0] != 'N'
		|| con->buffer[1] != 0) {
		printf ("2 con_buffer incorrect\n");
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

static int
test_3 (void)
{
	int         ret = 1;
	static char text[] = R"(01 don't forget this line
02 and some more lines here
03  adsf
04 adfa
06 hi there
06 don't forget there's line 07
)";
	static uint32_t lengths[] = { 26, 28, 9, 8, 12, 32, 0 };
	static uint32_t lengths2[] = { 1, 5, 2 };

	__auto_type con = Con_CreateBuffer (256, 8);
	Con_BufferAddText (con, text);

	if (con->line_head != 7) {
		printf ("con_buffer line_head incorrect: %d\n", con->line_head);
		goto fail;
	}
	if (con->line_tail != 0) {
		printf ("con_buffer line_tail incorrect: %d\n", con->line_tail);
		goto fail;
	}
	uint32_t    offs = 0;
	for (uint32_t i = 0; i < 7; i++) {
		if (con->lines[i].text != offs) {
			printf ("con_buffer lines[%d].text incorrect: %u %u\n",
					i, con->lines[i].text, offs);
			goto fail;
		}
		if (con->lines[i].len != lengths[i]) {
			printf ("con_buffer lines[%d].len incorrect: %u %u\n",
					i, con->lines[i].len, lengths[i]);
			goto fail;
		}
		offs += lengths[i];
	}

	Con_BufferAddText (con, "\nasdf\nas");
	if (con->line_head != 1) {
		printf ("2 con_buffer line_head incorrect: %d\n", con->line_head);
		goto fail;
	}
	if (con->line_tail != 2) {
		printf ("2 con_buffer line_tail incorrect: %d\n", con->line_tail);
		goto fail;
	}
	offs = lengths[0] + lengths[1];
	for (uint32_t i = 0; i < 4; i++) {
		uint32_t    n = i + 2;
		if (con->lines[n].text != offs) {
			printf ("2 con_buffer lines[%d].text incorrect: %u %u\n",
					n, con->lines[n].text, offs);
			goto fail;
		}
		if (con->lines[n].len != lengths[n]) {
			printf ("2 con_buffer lines[%d].len incorrect: %u %u\n",
					n, con->lines[n].len, lengths[n]);
			goto fail;
		}
		offs += lengths[n];
	}
	for (uint32_t i = 0; i < 3; i++) {
		uint32_t    n = (i + 6) % con->max_lines;
		if (con->lines[n].text != offs) {
			printf ("3 con_buffer lines[%d].text incorrect: %u %u\n",
					n, con->lines[n].text, offs);
			goto fail;
		}
		if (con->lines[n].len != lengths2[i]) {
			printf ("3 con_buffer lines[%d].len incorrect: %u %u\n",
					n, con->lines[n].len, lengths2[i]);
			goto fail;
		}
		offs += lengths2[i];
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
	ret |= test_3 ();
	return ret;
}
