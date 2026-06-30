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

	return res;
}
