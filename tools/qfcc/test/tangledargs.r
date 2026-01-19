#include "test-harness.h"

string vsprintf (string fmt, @va_list args) = #0;
void str_free (string str) = #0;
string str_hold (string str) = #0;


typedef struct obj_s {
	string filePath;
} obj_t;

string
get_name (obj_t *item)
{
	return item.filePath;
}

string QFS_CompressPath (string path)
{
	return path;
}

string result;

void
tprintf (string fmt, ...)
{
	string str = vsprintf (fmt, @args);
	if (fmt == "before:%s\n") {
		result = str_hold (str);
		printf ("str = %s\n", str);
	}
}

obj_t *
do_path (obj_t *self, obj_t *item)
{
	string path = self.filePath + "/" + get_name (item);
	str_free (self.filePath);
	tprintf ("before:%s\n", path);
	path = QFS_CompressPath (path);
	tprintf ("after:%s\n", path);
	self.filePath = str_hold (path);
	return self;
}

int
main ()
{
	obj_t obj = { .filePath = "here" };
	obj_t item = { .filePath = "there" };
	do_path (&obj, &item);
	return result != "before:here/there\n";
}
