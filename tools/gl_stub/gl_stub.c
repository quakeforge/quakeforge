#include <stdlib.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include "QF/GL/defines.h"
#include "QF/GL/extensions.h"
#include "QF/GL/types.h"

#include "QF/hash.h"

typedef XID GLXDrawable;
typedef struct __GLXcontextRec *GLXContext;

#define QFGL_DONT_NEED(ret, func, params) QFGL_NEED(ret, func, params)

#undef QFGL_NEED

#define QFGL_NEED(ret, name, args) \
ret GLAPIENTRY norm_##name args;
#include "QF/GL/qf_funcs_list.h"
#include "glx_funcs_list.h"
#undef QFGL_NEED

#define QFGL_NEED(ret, name, args) \
ret GLAPIENTRY trace_##name args;
#include "QF/GL/qf_funcs_list.h"
#include "glx_funcs_list.h"
#undef QFGL_NEED

typedef struct {
	const char *name;
	void       *norm;
	void       *trace;
} gl_stub_t;

static gl_stub_t gl_stub_funcs[] = {
#define QFGL_NEED(ret, name, args) \
	{#name, norm_##name, trace_##name},
#include "QF/GL/qf_funcs_list.h"
#include "glx_funcs_list.h"
#undef QFGL_NEED
};

#define NUM_FUNCS (sizeof (gl_stub_funcs) / sizeof (gl_stub_funcs[0]))

static int
cmp (const void *a, const void *b)
{
	return strcmp (((gl_stub_t *)a)->name, ((gl_stub_t *)b)->name);
}

void *
glXGetProcAddressARB (const GLubyte *procName)
{
	static int  called;
	gl_stub_t  *stub;
	gl_stub_t   key;

	if (!called) {
		qsort (gl_stub_funcs, NUM_FUNCS, sizeof (gl_stub_t), cmp);
		called = 1;
	}
	key.name = procName;
	stub = bsearch (&key, gl_stub_funcs, NUM_FUNCS, sizeof (gl_stub_t), cmp);
	if (!stub)
		return 0;
	return stub->norm;
}
