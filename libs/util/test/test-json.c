#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <string.h>
#include "QF/plist.h"

static const char *test_strings[] = {
	"{"                                                                   "\n"
	"    \"Image\": {"                                                    "\n"
	"        \"Width\":  800,"                                            "\n"
	"        \"Height\": 600,"                                            "\n"
	"        \"Title\":  \"View from 15th Floor\","                       "\n"
	"        \"Thumbnail\": {"                                            "\n"
	"            \"Url\":    \"http://www.example.com/image/481989943\"," "\n"
	"            \"Height\": 125,"                                        "\n"
	"            \"Width\":  100"                                         "\n"
	"        },"                                                          "\n"
	"        \"Animated\" : false,"                                       "\n"
	"        \"IDs\": [116, 943, 234, 38793],"                            "\n"
	"        \"ref\": null"                                               "\n"
	"    }"                                                               "\n"
	"}"                                                                   "\n",

	"["                                                                   "\n"
	"    {"                                                               "\n"
	"        \"precision\": \"zip\","                                     "\n"
	"        \"Latitude\":  37.7668,"                                     "\n"
	"        \"Longitude\": -122.3959,"                                   "\n"
	"        \"Address\":   \"\","                                        "\n"
	"        \"City\":      \"SAN FRANCISCO\","                           "\n"
	"        \"State\":     \"CA\","                                      "\n"
	"        \"Zip\":       \"94107\","                                   "\n"
	"        \"Country\":   \"US\""                                       "\n"
	"    },"                                                              "\n"
	"    {"                                                               "\n"
	"        \"precision\": \"zip\","                                     "\n"
	"        \"Latitude\":  37.371991,"                                   "\n"
	"        \"Longitude\": -122.026020,"                                 "\n"
	"        \"Address\":   \"\","                                        "\n"
	"        \"City\":      \"SUNNYVALE\","                               "\n"
	"        \"State\":     \"CA\","                                      "\n"
	"        \"Zip\":       \"94085\","                                   "\n"
	"        \"Country\":   \"US\""                                       "\n"
	"    }"                                                               "\n"
	"]"                                                                   "\n",

	"\"Hello world!\"",

	"42",

	"true",
};
#define num_string_tests (sizeof (test_strings) / sizeof (test_strings[0]))

static int
test_string_io (const char *str)
{
	plitem_t   *item;
	const char *res;
	char       *saved;

	item = PL_NewString (str);
	saved = PL_WriteJSON (item);
	PL_Release (item);
	item = PL_ParseJSON (saved, 0);
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
