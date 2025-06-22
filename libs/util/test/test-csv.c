#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <string.h>
#include <ctype.h>
#include "QF/plist.h"

static const char *test_strings[] = {
	//NOTE: this is one long line
	"030000004c050000e60c000011810000,PS5 Controller,a:b0,b:b1,back:b8,"
		"dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,guide:b10,"
		"leftshoulder:b4,leftstick:b11,lefttrigger:a2,leftx:a0,lefty:a1,"
		"rightshoulder:b5,rightstick:b12,righttrigger:a5,rightx:a3,"
		"righty:a4,start:b9,x:b3,y:b2,platform:Linux,",
};
#define num_string_tests (sizeof (test_strings) / sizeof (test_strings[0]))

static int __attribute__((pure,used))
wsstrcmp (const char *s1, const char *s2)
{
	while (*s1 && *s1 == *s2) {
		for (; *s1 && *s1 == *s2; s1++, s2++) continue;
		for (; *s1 && isspace (*s1); s1++) continue;
		for (; *s2 && isspace (*s2); s2++) continue;
	}
	return *s1 - *s2;
}

static void
free_item (plitem_t **item)
{
	if (*item) {
		PL_Release (*item);
	}
}
#define pldeferred __attribute__((cleanup (free_item)))

static int
test_string_io (const char *str)
{
	pldeferred plitem_t *item = PL_ParseCSV (str);
	if (!item) {
		printf ("failed to parse\n");
		return 0;
	}
	if (PL_Type (item) != QFArray) {
		printf ("not an array\n");
		return 0;
	}
	for (int i = 0; i < PL_A_NumObjects (item); i++) {
		auto str = PL_ObjectAtIndex (item, i);
		if (PL_Type (str) != QFString) {
			printf ("not an array\n");
			return 0;
		}
		printf ("%d: %s\n", i, PL_String (str));
	}
	return 1;
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
