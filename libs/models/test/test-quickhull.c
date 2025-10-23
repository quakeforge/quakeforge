#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "QF/dstring.h"
#include "QF/sys.h"

#include "quickhull.h"

static void
make_ico (vec4f_t *verts)
{
	float p = (sqrt(5) + 1) / 2;
	float a = sqrt (3) / p;
	float b = a / p;
	for (int i = 0; i < 3; i++) {
		for (int j = 0; j < 4; j++) {
			float my = j & 1 ? a : -a;
			float mz = j & 2 ? b : -b;
			int   vind = i * 4 + j;
			int   ix = i;
			int   iy = (i + 1) % 3;
			int   iz = (i + 2) % 3;
			verts[vind][ix] = 0;
			verts[vind][iy] = my;
			verts[vind][iz] = mz;
			verts[vind][3] = 1;
		}
	}
}

int
main ()
{
	auto qhull = quickhull_new_context (12);
	make_ico (qhull->verts);
	long start = Sys_LongTime ();
	quickhull_make_hull (qhull);
	long end = Sys_LongTime ();
	printf ("%dus\n", (int)(end - start));
	quickhull_delete_context (qhull);
	return 1;
}
