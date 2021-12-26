#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#define remove remove_renamed
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "QF/listener.h"
#include "QF/sys.h"
#undef remove

typedef struct obj_s {
	int count;
} obj_t;

struct LISTENER_SET_TYPE (obj_t) listeners = LISTENER_SET_STATIC_INIT(8);

static void
listener_a (void *data, const obj_t *obj)
{
	*(int *) data += 1;
	((obj_t *) obj)->count += 1;
}

static void
listener_b (void *data, const obj_t *obj)
{
	*(int *) data += 2;
	((obj_t *) obj)->count += 2;
}

int
main (int argc, const char **argv)
{
	int         res = 0;
	int         a_count = 0;
	int         b_count = 0;
	obj_t       obj = { 0 };

	LISTENER_ADD (&listeners, listener_a, &a_count);
	LISTENER_ADD (&listeners, listener_b, &b_count);
	LISTENER_INVOKE (&listeners, &obj);
	if (a_count != 1 || b_count != 2) {
		Sys_Error ("a_count = %d, b_count = %d", a_count, b_count);
	}
	if (obj.count != 3) {
		Sys_Error ("obj.count = %d", obj.count);
	}
	LISTENER_REMOVE (&listeners, listener_b, &b_count);
	LISTENER_INVOKE (&listeners, &obj);
	if (a_count != 2 || b_count != 2) {
		Sys_Error ("a_count = %d, b_count = %d", a_count, b_count);
	}
	if (obj.count != 4) {
		Sys_Error ("obj.count = %d", obj.count);
	}
	return res;
}
