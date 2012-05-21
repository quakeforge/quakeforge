/*
 * quaketoascii - Convert Quake extended characters into ASCII
 *
 * This program is public domain.
 *
 * The orginal author is Dwayne C. Litzenberger <dlitz@dlitz.net>.
 * THIS SOFTWARE IS PROVIDED AS-IS WITHOUT WARRANTY
 * OF ANY KIND, NOT EVEN THE IMPLIED WARRANTY OF
 * MERCHANTABILITY. THE AUTHOR OF THIS SOFTWARE,
 * ASSUMES _NO_ RESPONSIBILITY FOR ANY CONSEQUENCE
 * RESULTING FROM THE USE, MODIFICATION, OR
 * REDISTRIBUTION OF THIS SOFTWARE.
 *
 */

#include <stdio.h>
#include <errno.h>

#include "QF/sys.h"

int
main (int argc, char *argv[])
{
	unsigned char	c;

	if (argc > 1) {
		printf ("quaketoascii - Convert Quake extended characters into ASCII\n");
		printf ("Usage: %s < infile > outfile\n", argv[0]);
		return 1;
	}

	while (!feof (stdin) && !ferror (stdin)) {
		if (fread (&c, 1, 1, stdin) == 1) {
			fputc (sys_char_map[(int) c], stdout);
			fflush (stdout);
		}
	}
	if (ferror (stdin)) {
		perror ("fread");
		return 1;
	}
	return 0;
}
