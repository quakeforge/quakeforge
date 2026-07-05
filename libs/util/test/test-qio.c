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

#include "QF/quakeio.h"

static const char test_text[] = R"(
I'm not at all sure what's going to happen here, but what I do know is that
things will be tested.
)";

int
main (int argc, const char **argv)
{
	int         res = 0;
	QFile      *file = Qmemopen (test_text, sizeof (test_text), 0);

	if (!file) {
		printf ("could not open memfile\n");
		return 1;
	}
	if (Qfilesize (file) != sizeof (test_text)) {
		printf ("memfile size wrong: %zd != %zd\n", Qfilesize (file),
				sizeof (test_text));
		res = 1;
	}
	char buf[sizeof (test_text) * 2];
	size_t bytes = Qread (file, buf, sizeof(buf) * 2);
	if (bytes != Qfilesize (file)) {
		printf ("memfile read wrong: %zd != %zd\n", bytes, Qfilesize (file));
		res = 1;
	}
	if (strcmp (buf, test_text) != 0) {
		printf ("memfile read corrupt: '%s'\n", buf);
		res = 1;
	}
	Qclose (file);

	char tmpname[] = "tmpXXXXXX";
	int fd = mkstemp (tmpname);
	if (fd < 0) {
		printf ("could not create tmp file\n");
		return 1;
	}
	file = Qdopen (fd, "wbz9");
	Qputs (file, test_text);
	Qclose (file);

	file = Qopen (tmpname, "rb");
	unsigned char gzbuf[Qfilesize (file)];
	Qread (file, gzbuf, sizeof (gzbuf));
	Qclose (file);

	file = Qmemopen (gzbuf, sizeof (gzbuf), 1);
	char debuf[sizeof (test_text)] = {};
	Qread (file, debuf, sizeof (debuf));
	if (strcmp (buf, test_text) != 0) {
		printf ("memfile read corrupt: '%.*s'\n", (int) sizeof (debuf), debuf);
		res = 1;
	}
	Qclose (file);

	remove (tmpname);

	return res;
}
