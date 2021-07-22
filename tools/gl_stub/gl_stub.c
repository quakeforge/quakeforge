#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include "QF/GL/defines.h"
#include "QF/GL/extensions.h"
#include "QF/GL/types.h"
#include "QF/GLSL/types.h"

#include "QF/hash.h"

typedef XID GLXDrawable;
typedef struct __GLXcontextRec *GLXContext;

#define TRACE do { \
	puts (__FUNCTION__);\
} while (0)

#define QFGL_DONT_NEED(ret, func, params) QFGL_NEED(ret, func, params)

#undef QFGL_WANT
#undef QFGL_NEED

#define QFGL_WANT(ret, name, args) \
ret GLAPIENTRY norm_##name args;
#define QFGL_NEED(ret, name, args) \
ret GLAPIENTRY norm_##name args;
#include "QF/GL/qf_funcs_list.h"
#include "QF/GLSL/qf_funcs_list.h"
#undef QFGL_NEED
#undef QFGL_WANT

#define QFGL_WANT(ret, name, args) \
ret GLAPIENTRY trace_##name args;
#define QFGL_NEED(ret, name, args) \
ret GLAPIENTRY trace_##name args;
#include "QF/GL/qf_funcs_list.h"
#include "QF/GLSL/qf_funcs_list.h"
#undef QFGL_NEED
#undef QFGL_WANT

typedef struct {
	const char *name;
	void       *norm;
	void       *trace;
} gl_stub_t;

static int  trace;

static gl_stub_t gl_stub_funcs[] = {
#define QFGL_WANT(ret, name, args) \
	{#name, norm_##name, trace_##name},
#define QFGL_NEED(ret, name, args) \
	{#name, norm_##name, trace_##name},
#include "QF/GL/qf_funcs_list.h"
#include "QF/GLSL/qf_funcs_list.h"
#undef QFGL_NEED
#undef QFGL_WANT
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
		char       *glstub_trace;

		qsort (gl_stub_funcs, NUM_FUNCS, sizeof (gl_stub_t), cmp);
		called = 1;

		glstub_trace = getenv ("GLSTUB_TRACE");
		if (glstub_trace)
			trace = atoi (glstub_trace);
	}
	key.name = (const char *) procName;
	stub = bsearch (&key, gl_stub_funcs, NUM_FUNCS, sizeof (gl_stub_t), cmp);
	if (!stub)
		return 0;
	return trace ? stub->trace : stub->norm;
}

void
glXSwapBuffers (Display *dpy, GLXDrawable drawable)
{
	if (trace)
		TRACE;
}

XVisualInfo*
glXChooseVisual (Display *dpy, int screen, int *attribList)
{
	XVisualInfo template;
	int         num_visuals;
	int         template_mask;

	if (trace)
		TRACE;

	template_mask = 0;
	template.visualid =
		XVisualIDFromVisual (XDefaultVisual (dpy, screen));
	template_mask = VisualIDMask;
	return XGetVisualInfo (dpy, template_mask, &template, &num_visuals);
}

GLXContext
glXCreateContext (Display *dpy, XVisualInfo *vis, GLXContext shareList,
				  Bool direct)
{
	if (trace)
		TRACE;
	return (GLXContext)1;
}

Bool
glXMakeCurrent (Display *dpy, GLXDrawable drawable, GLXContext ctx)
{
	if (trace)
		TRACE;
	return 1;
}

void
glXDestroyContext (Display *dpy, GLXContext ctx )
{
	if (trace)
		TRACE;
}

int
glXGetConfig (Display *dpy, XVisualInfo *visual, int attrib, int *value )
{
	if (trace)
		TRACE;
    return 0;
}
