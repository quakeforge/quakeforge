#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "QF/sys.h"

#include "QF/GL/defines.h"
#include "QF/GL/funcs.h"

typedef void (*qfgl_func_t) (const GLvoid *);

static const GLvoid *color_pointer;
static GLsizei color_stride;
static qfgl_func_t color_func;
static int  color_enabled = 0;

static const GLvoid *edgeflag_pointer;
static GLsizei edgeflag_stride;
static int  edgeflag_enabled = 0;

static const GLvoid *index_pointer;
static GLsizei index_stride;
static qfgl_func_t index_func;
static int  index_enabled = 0;

static const GLvoid *normal_pointer;
static GLsizei normal_stride;
static qfgl_func_t normal_func;
static int  normal_enabled = 0;

static const GLvoid *texcoord_pointer;
static GLsizei texcoord_stride;
static qfgl_func_t texcoord_func;
static int  texcoord_enabled = 0;

static const GLvoid *vertex_pointer;
static GLsizei vertex_stride;
static qfgl_func_t vertex_func;
static int  vertex_enabled = 0;

static qfgl_func_t *color_functions[][8] = {
	{
		(qfgl_func_t *) (char *) &qfglColor3bv,
		(qfgl_func_t *) (char *) &qfglColor3ubv,
		(qfgl_func_t *) (char *) &qfglColor3sv,
		(qfgl_func_t *) (char *) &qfglColor3usv,
		(qfgl_func_t *) (char *) &qfglColor3iv,
		(qfgl_func_t *) (char *) &qfglColor3uiv,
		(qfgl_func_t *) (char *) &qfglColor3fv,
		(qfgl_func_t *) (char *) &qfglColor3dv,
	},
	{
		(qfgl_func_t *) (char *) &qfglColor4bv,
		(qfgl_func_t *) (char *) &qfglColor4ubv,
		(qfgl_func_t *) (char *) &qfglColor4sv,
		(qfgl_func_t *) (char *) &qfglColor4usv,
		(qfgl_func_t *) (char *) &qfglColor4iv,
		(qfgl_func_t *) (char *) &qfglColor4uiv,
		(qfgl_func_t *) (char *) &qfglColor4fv,
		(qfgl_func_t *) (char *) &qfglColor4dv,
	},
};

static qfgl_func_t *index_functions[] = {
	(qfgl_func_t *) (char *) &qfglIndexubv,
	(qfgl_func_t *) (char *) &qfglIndexsv,
	(qfgl_func_t *) (char *) &qfglIndexiv,
	(qfgl_func_t *) (char *) &qfglIndexfv,
	(qfgl_func_t *) (char *) &qfglIndexdv,
};

static qfgl_func_t *normal_functions[] = {
	(qfgl_func_t *) (char *) &qfglNormal3bv,
	(qfgl_func_t *) (char *) &qfglNormal3sv,
	(qfgl_func_t *) (char *) &qfglNormal3iv,
	(qfgl_func_t *) (char *) &qfglNormal3fv,
	(qfgl_func_t *) (char *) &qfglNormal3dv,
};

static qfgl_func_t *texcoord_functions[][4] = {
	{
		(qfgl_func_t *) (char *) &qfglTexCoord1sv,
		(qfgl_func_t *) (char *) &qfglTexCoord1iv,
		(qfgl_func_t *) (char *) &qfglTexCoord1fv,
		(qfgl_func_t *) (char *) &qfglTexCoord1dv,
	},
	{
		(qfgl_func_t *) (char *) &qfglTexCoord2sv,
		(qfgl_func_t *) (char *) &qfglTexCoord2iv,
		(qfgl_func_t *) (char *) &qfglTexCoord2fv,
		(qfgl_func_t *) (char *) &qfglTexCoord2dv,
	},
	{
		(qfgl_func_t *) (char *) &qfglTexCoord3sv,
		(qfgl_func_t *) (char *) &qfglTexCoord3iv,
		(qfgl_func_t *) (char *) &qfglTexCoord3fv,
		(qfgl_func_t *) (char *) &qfglTexCoord3dv,
	},
	{
		(qfgl_func_t *) (char *) &qfglTexCoord4sv,
		(qfgl_func_t *) (char *) &qfglTexCoord4iv,
		(qfgl_func_t *) (char *) &qfglTexCoord4fv,
		(qfgl_func_t *) (char *) &qfglTexCoord4dv,
	},
};

static qfgl_func_t *vertex_functions[][4] = {
	{
		(qfgl_func_t *) (char *) &qfglVertex2sv,
		(qfgl_func_t *) (char *) &qfglVertex2iv,
		(qfgl_func_t *) (char *) &qfglVertex2fv,
		(qfgl_func_t *) (char *) &qfglVertex2dv,
	},
	{
		(qfgl_func_t *) (char *) &qfglVertex3sv,
		(qfgl_func_t *) (char *) &qfglVertex3iv,
		(qfgl_func_t *) (char *) &qfglVertex3fv,
		(qfgl_func_t *) (char *) &qfglVertex3dv,
	},
	{
		(qfgl_func_t *) (char *) &qfglVertex4sv,
		(qfgl_func_t *) (char *) &qfglVertex4iv,
		(qfgl_func_t *) (char *) &qfglVertex4fv,
		(qfgl_func_t *) (char *) &qfglVertex4dv,
	},
};

static void
qfgl_ColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
	int         index;
	int         bytes = 0;

	switch (type) {
		case GL_BYTE:
			index = 0;
			bytes = sizeof (GLbyte);
			break;
		case GL_UNSIGNED_BYTE:
			index = 1;
			bytes = sizeof (GLubyte);
			break;
		case GL_SHORT:
			index = 2;
			bytes = sizeof (GLshort);
			break;
		case GL_UNSIGNED_SHORT:
			index = 3;
			bytes = sizeof (GLushort);
			break;
		case GL_INT:
			index = 4;
			bytes = sizeof (GLint);
			break;
		case GL_UNSIGNED_INT:
			index = 5;
			bytes = sizeof (GLuint);
			break;
		case GL_FLOAT:
			index = 6;
			bytes = sizeof (GLfloat);
			break;
		case GL_DOUBLE:
			index = 7;
			bytes = sizeof (GLdouble);
			break;
		default:
			return;
	}
	color_pointer = ptr;
//	color_stride = stride + size * bytes;
	color_stride = stride ? stride : size * bytes;
	color_func = *color_functions[size - 3][index];
}

static void
qfgl_EdgeFlagPointer (GLsizei stride, const GLvoid *ptr)
{
	edgeflag_pointer = ptr;
	edgeflag_stride = stride + sizeof (GLboolean);
}

static void
qfgl_IndexPointer (GLenum type, GLsizei stride, const GLvoid *ptr)
{
	int         index;
	int         bytes = 0;

	switch (type) {
		case GL_UNSIGNED_BYTE:
			index = 0;
			bytes = sizeof (GLubyte);
			break;
		case GL_SHORT:
			index = 1;
			bytes = sizeof (GLshort);
			break;
		case GL_INT:
			index = 2;
			bytes = sizeof (GLint);
			break;
		case GL_FLOAT:
			index = 3;
			bytes = sizeof (GLfloat);
			break;
		case GL_DOUBLE:
			index = 4;
			bytes = sizeof (GLdouble);
			break;
		default:
			return;
	}
	index_pointer = ptr;
	index_stride = stride + bytes;
	index_func = *index_functions[index];
}

static void
qfgl_NormalPointer (GLenum type, GLsizei stride, const GLvoid *ptr)
{
	int         index;
	int         bytes = 0;

	switch (type) {
		case GL_BYTE:
			index = 0;
			bytes = sizeof (GLbyte);
			break;
		case GL_SHORT:
			index = 1;
			bytes = sizeof (GLshort);
			break;
		case GL_INT:
			index = 2;
			bytes = sizeof (GLint);
			break;
		case GL_FLOAT:
			index = 3;
			bytes = sizeof (GLfloat);
			break;
		case GL_DOUBLE:
			index = 4;
			bytes = sizeof (GLdouble);
			break;
		default:
			return;
	}
	normal_pointer = ptr;
	normal_stride = stride + bytes;
	normal_func = *normal_functions[index];
}

static void
qfgl_TexCoordPointer (GLint size, GLenum type, GLsizei stride,
					  const GLvoid *ptr)
{
	int         index;
	int         bytes = 0;

	switch (type) {
		case GL_SHORT:
			index = 0;
			bytes = sizeof (GLshort);
			break;
		case GL_INT:
			index = 1;
			bytes = sizeof (GLint);
			break;
		case GL_FLOAT:
			index = 2;
			bytes = sizeof (GLfloat);
			break;
		case GL_DOUBLE:
			index = 3;
			bytes = sizeof (GLdouble);
			break;
		default:
			return;
	}
	texcoord_pointer = ptr;
	texcoord_stride = stride + size * bytes;
	texcoord_func = *texcoord_functions[size - 1][index];
}

static void
qfgl_VertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid *ptr)
{
	int         index;
	int         bytes = 0;

	switch (type) {
		case GL_SHORT:
			index = 0;
			bytes = sizeof (GLshort);
			break;
		case GL_INT:
			index = 1;
			bytes = sizeof (GLint);
			break;
		case GL_FLOAT:
			index = 2;
			bytes = sizeof (GLfloat);
			break;
		case GL_DOUBLE:
			index = 3;
			bytes = sizeof (GLdouble);
			break;
		default:
			return;
	}
	vertex_pointer = ptr;
	vertex_stride = stride + size * bytes;
	vertex_func = *vertex_functions[size - 2][index];
}

static void
qfgl_InterleavedArrays (GLenum format, GLsizei stride, const GLvoid *pointer)
{
	const GLbyte *ptr = pointer;

	switch (format) {
		case GL_V2F:
			stride += 2 * sizeof (GLfloat);
			break;
		case GL_V3F:
			stride += 3 * sizeof (GLfloat);
			break;
		case GL_C4UB_V2F:
			stride += 4 * sizeof (GLubyte);
			stride += 2 * sizeof (GLfloat);
			break;
		case GL_C4UB_V3F:
			stride += 4 * sizeof (GLubyte);
			stride += 3 * sizeof (GLfloat);
			break;
		case GL_C3F_V3F:
			stride += 3 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			break;
		case GL_N3F_V3F:
			stride += 3 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			break;
		case GL_C4F_N3F_V3F:
			stride += 4 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			break;
		case GL_T2F_V3F:
			stride += 2 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			break;
		case GL_T4F_V4F:
			stride += 4 * sizeof (GLfloat);
			stride += 4 * sizeof (GLfloat);
			break;
		case GL_T2F_C4UB_V3F:
			stride += 2 * sizeof (GLfloat);
			stride += 4 * sizeof (GLubyte);
			stride += 3 * sizeof (GLfloat);
			break;
		case GL_T2F_C3F_V3F:
			stride += 2 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			break;
		case GL_T2F_N3F_V3F:
			stride += 2 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			break;
		case GL_T2F_C4F_N3F_V3F:
			stride += 2 * sizeof (GLfloat);
			stride += 4 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			break;
		case GL_T4F_C4F_N3F_V4F:
			stride += 4 * sizeof (GLfloat);
			stride += 4 * sizeof (GLfloat);
			stride += 3 * sizeof (GLfloat);
			stride += 4 * sizeof (GLfloat);
			break;
		default:
			break;
	}

	switch (format) {
		case GL_T2F_V3F:
		case GL_T2F_C4UB_V3F:
		case GL_T2F_C3F_V3F:
		case GL_T2F_N3F_V3F:
		case GL_T2F_C4F_N3F_V3F:
			qfgl_TexCoordPointer (2, GL_FLOAT, stride - 2 * sizeof (GLfloat),
								  ptr);
			ptr += 2 * sizeof (GLfloat);
			break;
		case GL_T4F_V4F:
		case GL_T4F_C4F_N3F_V4F:
			qfgl_TexCoordPointer (4, GL_FLOAT, stride - 4 * sizeof (GLfloat),
								  ptr);
			ptr += 4 * sizeof (GLfloat);
			break;
		default:
			break;
	}
	switch (format) {
		case GL_C4UB_V2F:
		case GL_C4UB_V3F:
		case GL_T2F_C4UB_V3F:
			qfgl_ColorPointer (4, GL_UNSIGNED_BYTE,
							   stride - 4 * sizeof (GLubyte), ptr);
			ptr += 4 * sizeof (GLubyte);
			break;
		case GL_C3F_V3F:
		case GL_T2F_C3F_V3F:
			qfgl_ColorPointer (3, GL_FLOAT, stride - 3 * sizeof (GLfloat),
							   ptr);
			ptr += 3 * sizeof (GLfloat);
			break;
		case GL_C4F_N3F_V3F:
		case GL_T2F_C4F_N3F_V3F:
		case GL_T4F_C4F_N3F_V4F:
			qfgl_ColorPointer (4, GL_FLOAT, stride - 4 * sizeof (GLfloat),
							   ptr);
			ptr += 4 * sizeof (GLfloat);
			break;
		default:
			break;
	}
	switch (format) {
		case GL_N3F_V3F:
		case GL_C4F_N3F_V3F:
		case GL_T2F_N3F_V3F:
		case GL_T2F_C4F_N3F_V3F:
		case GL_T4F_C4F_N3F_V4F:
			qfgl_NormalPointer (GL_FLOAT, stride - 3 * sizeof (GLfloat), ptr);
			ptr += 3 * sizeof (GLfloat);
			break;
		default:
			break;
	}
	switch (format) {
		case GL_V2F:
		case GL_C4UB_V2F:
			qfgl_VertexPointer (2, GL_FLOAT, stride - 2 * sizeof (GLfloat),
								ptr);
			break;
		case GL_V3F:
		case GL_C4UB_V3F:
		case GL_C3F_V3F:
		case GL_N3F_V3F:
		case GL_C4F_N3F_V3F:
		case GL_T2F_V3F:
		case GL_T2F_C4UB_V3F:
		case GL_T2F_C3F_V3F:
		case GL_T2F_N3F_V3F:
		case GL_T2F_C4F_N3F_V3F:
			qfgl_VertexPointer (3, GL_FLOAT, stride - 3 * sizeof (GLfloat),
								ptr);
			break;
		case GL_T4F_V4F:
		case GL_T4F_C4F_N3F_V4F:
			qfgl_VertexPointer (4, GL_FLOAT, stride - 4 * sizeof (GLfloat),
								ptr);
			break;
		default:
			break;
	}
}

static void
qfgl_ArrayElement (GLint i)
{
	if (texcoord_enabled)
		texcoord_func ((char*)texcoord_pointer + i * texcoord_stride);
	if (edgeflag_enabled)
		qfglEdgeFlagv ((byte*)edgeflag_pointer + i * edgeflag_stride);
	if (color_enabled)
		color_func ((char*)color_pointer + i * color_stride);
	if (normal_enabled)
		normal_func ((char*)normal_pointer + i * normal_stride);
	if (index_enabled)
		index_func ((char*)index_pointer + i * index_stride);
	if (vertex_enabled)
		vertex_func ((char*)vertex_pointer + i * vertex_stride);
}

static void
qfgl_DrawArrays (GLenum mode, GLint first, GLsizei count)
{
	GLint       i;

	qfglBegin (mode);
	for (i = first; i < first + count; i++)
		qfgl_ArrayElement (i);
	qfglEnd ();
}

static void
qfgl_DrawElements (GLenum mode, GLsizei count, GLenum type,
				   const GLvoid * indices)
{
	GLsizei     i;
	const GLubyte  *ub_indices;
	const GLushort *us_indices;
	const GLuint   *ui_indices;

	switch (type) {
		case GL_UNSIGNED_BYTE:
			ub_indices = indices;
			for (i = 0; i < count; i++)
				qfgl_ArrayElement (ub_indices[i]);
			break;
		case GL_UNSIGNED_SHORT:
			us_indices = indices;
			for (i = 0; i < count; i++)
				qfgl_ArrayElement (us_indices[i]);
			break;
		case GL_UNSIGNED_INT:
			ui_indices = indices;
			for (i = 0; i < count; i++)
				qfgl_ArrayElement (ui_indices[i]);
			break;
		default:
			break;
	}
}

static void
qfgl_DrawRangeElements (GLenum mode, GLuint start, GLuint end, GLsizei count,
						GLenum type, const GLvoid * indices)
{
	GLsizei     i;
	const GLubyte  *ub_indices;
	const GLushort *us_indices;
	const GLuint   *ui_indices;

	switch (type) {
		case GL_UNSIGNED_BYTE:
			ub_indices = indices;
			for (i = 0; i < count; i++)
				if (ub_indices[i] >= start && ub_indices[i] <= end)
					qfgl_ArrayElement (ub_indices[i]);
			break;
		case GL_UNSIGNED_SHORT:
			us_indices = indices;
			for (i = 0; i < count; i++)
				if (us_indices[i] >= start && us_indices[i] <= end)
					qfgl_ArrayElement (us_indices[i]);
			break;
		case GL_UNSIGNED_INT:
			ui_indices = indices;
			for (i = 0; i < count; i++)
				if (ui_indices[i] >= start && ui_indices[i] <= end)
					qfgl_ArrayElement (ui_indices[i]);
			break;
		default:
			break;
	}
}

static void
client_state (GLenum cap, GLboolean state)
{
	switch (cap) {
		case GL_VERTEX_ARRAY:
			vertex_enabled = state;
			break;
		case GL_NORMAL_ARRAY:
			normal_enabled = state;
			break;
		case GL_COLOR_ARRAY:
			color_enabled = state;
			break;
		case GL_INDEX_ARRAY:
			index_enabled = state;
			break;
		case GL_TEXTURE_COORD_ARRAY:
			texcoord_enabled = state;
			break;
		case GL_EDGE_FLAG_ARRAY:
			edgeflag_enabled = state;
			break;
	}
}

static void
qfgl_EnableClientState (GLenum cap)
{
	client_state (cap, GL_TRUE);
}

static void
qfgl_DisableClientState (GLenum cap)
{
	client_state (cap, GL_FALSE);
}

static void
qfgl_GetPointerv (GLenum pname, void **params)
{
	Sys_Error ("GetPointerv not implemented");
}

static void
qfgl_PopClientAttrib (void)
{
	Sys_Error ("PopClientAttrib not implemented");
}

static void
qfgl_PushClientAttrib (GLbitfield mask)
{
	Sys_Error ("PushClientAttrib not implemented");
}

struct {
	const char *name;
	void       *func;
} qfgl_functions [] = {
	{"glColorPointer", qfgl_ColorPointer},
	{"glEdgeFlagPointer", qfgl_EdgeFlagPointer},
	{"glIndexPointer", qfgl_IndexPointer},
	{"glNormalPointer", qfgl_NormalPointer},
	{"glTexCoordPointer", qfgl_TexCoordPointer},
	{"glVertexPointer", qfgl_VertexPointer},
	{"glInterleavedArrays", qfgl_InterleavedArrays},
	{"glArrayElement", qfgl_ArrayElement},
	{"glDrawArrays", qfgl_DrawArrays},
	{"glDrawElements", qfgl_DrawElements},
	{"glDrawRangeElements", qfgl_DrawRangeElements},
	{"glEnableClientState", qfgl_EnableClientState},
	{"glDisableClientState", qfgl_DisableClientState},
	{"glGetPointerv", qfgl_GetPointerv},
	{"glPopClientAttrib", qfgl_PopClientAttrib},
	{"glPushClientAttrib", qfgl_PushClientAttrib},
};
