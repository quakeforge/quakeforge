/*
	gl_stub.c

	Dummy glXGetProcAddressARB function

	Copyright (C)2001 Bill Currie <bill@taniwha.org>
	Copyright (C)2002 Chris Ison wildcode@users.sourceforge.net

	This file is part of Quakeforge and lib3Dfxgl.

	Quakeforge and lib3Dfxgl are free software; you can redistribute them
	and/or modify them under the terms of the GNU General Public License
	as published by	the Free Software Foundation; either version 2 of the
	License, or (at your option) any later version.

	Quakeforge and lib3Dfxgl are distributed in the hope that they will be
	useful,	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

	See the	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Quakeforge and lib3Dfxgl; if not, write to:

		Free Software Foundation, Inc.
		59 Temple Place - Suite 330
		Boston, MA  02111-1307, USA

*/


#include <stdlib.h>
#include "3dfxstub.h"
#include "QF/GL/defines.h"
#include "QF/GL/extensions.h"
#include "QF/GL/types.h"

#include "QF/hash.h"

#define QFGL_DONT_NEED(ret, func, params) QFGL_NEED(ret, func, params)

#undef QFGL_WANT
#undef QFGL_NEED
#define QFGL_WANT(ret, name, args) \
ret GLAPIENTRY norm_##name args;
#define QFGL_NEED(ret, name, args) \
ret GLAPIENTRY norm_##name args;
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED
#undef QFGL_WANT
fxMesaContext norm_fxMesaCreateContext (GLuint win, GrScreenResolution_t res, GrScreenRefresh_t ref, const GLint attribList[]);
void norm_fxMesaMakeCurrent(fxMesaContext fxMesa);
void norm_fxMesaDestroyContext(fxMesaContext fxMesa);
void norm_fxMesaSwapBuffers(void);

#define QFGL_WANT(ret, name, args) \
ret GLAPIENTRY trace_##name args;
#define QFGL_NEED(ret, name, args) \
ret GLAPIENTRY trace_##name args;
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED
#undef QFGL_WANT
fxMesaContext trace_fxMesaCreateContext (GLuint win, GrScreenResolution_t res, GrScreenRefresh_t ref, const GLint attribList[]);
void trace_fxMesaMakeCurrent(fxMesaContext fxMesa);
void trace_fxMesaDestroyContext(fxMesaContext fxMesa);
void trace_fxMesaSwapBuffers(void);

typedef struct {
	const char *name;
	void       *norm;
	void       *trace;
} gl_stub_t;

static gl_stub_t gl_stub_funcs[] = {
#define QFGL_WANT(ret, name, args) \
	{#name, norm_##name, trace_##name},
#define QFGL_NEED(ret, name, args) \
	{#name, norm_##name, trace_##name},
#include "QF/GL/qf_funcs_list.h"
#undef QFGL_NEED
#undef QFGL_WANT
	{"fxMesaCreateContext", norm_fxMesaCreateContext, trace_fxMesaCreateContext},
	{"fxMesaMakeCurrent", norm_fxMesaMakeCurrent, trace_fxMesaMakeCurrent},
	{"fxMesaDestroyContext", norm_fxMesaDestroyContext, trace_fxMesaDestroyContext},
	{"fxMesaSwapBuffers", norm_fxMesaSwapBuffers, trace_fxMesaSwapBuffers},
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
	static int  trace;
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
	key.name = procName;
	stub = bsearch (&key, gl_stub_funcs, NUM_FUNCS, sizeof (gl_stub_t), cmp);
	if (!stub)
		return 0;
	return trace ? stub->trace : stub->norm;
}
