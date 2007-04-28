#include <Object.h>
integer zero;
string (Object foo) getstr;

id (Object foo) bar =
{
	[foo error:"foo %s", getstr(foo)];
};
