#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <string.h>
#include "QF/qfplist.h"

static const char *test_strings[] = {
	"Guarding the entrance to the Grendal\n"
	"Gorge is the Shadow Gate, a small keep\n"
	"and monastary which was once the home\n"
	"of the Shadow cult.\n\n"
	"For years the Shadow Gate existed in\n"
	"obscurity but after the cult discovered\n"
	"the \302\354\341\343\353\240\307\341\364\345 in the caves below\n"
	"the empire took notice.\n"
	"A batallion of Imperial Knights were\n"
	"sent to the gate to destroy the cult\n"
	"and claim the artifact for the King.",
};
#define num_string_tests (sizeof (test_strings) / sizeof (test_strings[0]))

static int
test_string_io (const char *str)
{
	plitem_t   *item;
	const char *res;
	char       *saved;

	item = PL_NewString (str);
	saved = PL_WritePropertyList (item);
	PL_Free (item);
	item = PL_GetPropertyList (saved);
	res = PL_String (item);
	if (!strcmp (str, res))
		return 1;
	printf ("expect: %s\n", str);
	printf ("got   : %s\n", res);
	printf ("saved : %s\n", saved);
	return 0;
}

int
main (int argc, const char **argv)
{
	int         res = 0;
	size_t      i;

	for (i = 0; i < num_string_tests; i ++) {
		if (!test_string_io (test_strings[i]))
			res = 1;
	}
	return res;
}
