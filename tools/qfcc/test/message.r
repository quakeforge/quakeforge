#include <Object.h>
integer zero;
string (Object foo) getstr;

id (Object foo) bar =
{
	return [foo error:"foo %s", getstr(foo)];
};
