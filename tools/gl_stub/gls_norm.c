#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include "QF/GL/defines.h"
#include "QF/GL/types.h"
#include "QF/GLSL/types.h"

typedef struct __GLXcontextRec *GLXContext;
typedef XID GLXDrawable;

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

void
norm_glAccum (GLenum op, GLfloat value)
{
}

void
norm_glActiveTexture (GLenum texture)
{
}

void
norm_glAlphaFunc (GLenum func, GLclampf ref)
{
}

GLboolean
norm_glAreTexturesResident (GLsizei n, const GLuint * textures,
					   GLboolean * residences)
{
	return 0;
}

void
norm_glArrayElement (GLint i)
{
}

void
norm_glAttachShader (GLuint program, GLuint shader)
{
}

void
norm_glBegin (GLenum mode)
{
}

void
norm_glBindAttribLocation (GLuint program, GLuint index, const GLchar* name)
{
}

void
norm_glBindBuffer (GLenum target, GLuint buffer)
{
}

void
norm_glBindFramebuffer (GLenum target, GLuint framebuffer)
{
}

void
norm_glBindRenderbuffer (GLenum target, GLuint renderbuffer)
{
}

void
norm_glBindTexture (GLenum target, GLuint texture)
{
}

void
norm_glBitmap (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig,
		  GLfloat xmove, GLfloat ymove, const GLubyte * bitmap)
{
}

void
norm_glBlendColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
}

void
norm_glBlendEquation (GLenum mode)
{
}

void
norm_glBlendEquationSeparate (GLenum modeRGB, GLenum modeAlpha)
{
}

void
norm_glBlendFunc (GLenum sfactor, GLenum dfactor)
{
}

void
norm_glBlendFuncSeparate (GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
}

void
norm_glBufferData (GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
}

void
norm_glBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
}

void
norm_glCallList (GLuint list)
{
}

void
norm_glCallLists (GLsizei n, GLenum type, const GLvoid * lists)
{
}

GLenum
norm_glCheckFramebufferStatus (GLenum target)
{
	return 0x8CD5;
}

void
norm_glClearDepthf (GLclampf depth)
{
}

void
norm_glCompileShader (GLuint shader)
{
}

void
norm_glCompressedTexImage2D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid* data)
{
}

void
norm_glCompressedTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid* data)
{
}

GLuint
norm_glCreateProgram (void)
{
	static int program;
	return ++program;
}

GLuint
norm_glCreateShader (GLenum type)
{
	static int shader;
	return ++shader;
}

void
norm_glDeleteBuffers (GLsizei n, const GLuint* buffers)
{
}

void
norm_glDeleteFramebuffers (GLsizei n, const GLuint* framebuffers)
{
}

void
norm_glDeleteProgram (GLuint program)
{
}

void
norm_glDeleteRenderbuffers (GLsizei n, const GLuint* renderbuffers)
{
}

void
norm_glDeleteShader (GLuint shader)
{
}

void
norm_glDepthRangef (GLclampf zNear, GLclampf zFar)
{
}

void
norm_glDetachShader (GLuint program, GLuint shader)
{
}

void
norm_glDisableVertexAttribArray (GLuint index)
{
}

void
norm_glEnableVertexAttribArray (GLuint index)
{
}

void
norm_glFramebufferRenderbuffer (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
}

void
norm_glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
}

void
norm_glGenBuffers (GLsizei n, GLuint* buffers)
{
}

void
norm_glGenFramebuffers (GLsizei n, GLuint* framebuffers)
{
}

void
norm_glGenRenderbuffers (GLsizei n, GLuint* renderbuffers)
{
}

void
norm_glGenerateMipmap (GLenum target)
{
}

void
norm_glGetActiveAttrib (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
}

void
norm_glGetActiveUniform (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
}

void
norm_glGetAttachedShaders (GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders)
{
}

int
norm_glGetAttribLocation (GLuint program, const GLchar* name)
{
	return 0;
}

void
norm_glGetBufferParameteriv (GLenum target, GLenum pname, GLint* params)
{
}

void
norm_glGetFramebufferAttachmentParameteriv (GLenum target, GLenum attachment, GLenum pname, GLint* params)
{
}

void
norm_glGetProgramInfoLog (GLuint program, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
}

void
norm_glGetProgramiv (GLuint program, GLenum pname, GLint* params)
{
}

void
norm_glGetRenderbufferParameteriv (GLuint shader, GLenum pname, GLint* params)
{
}

void
norm_glGetShaderInfoLog (GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
}

void
norm_glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision)
{
}

void
norm_glGetShaderSource (GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* source)
{
}

void
norm_glGetShaderiv (GLuint shader, GLenum pname, GLint* params)
{
}

int
norm_glGetUniformLocation (GLuint program, const GLchar* name)
{
	return 0;
}

void
norm_glGetUniformfv (GLuint program, GLint location, GLfloat* params)
{
}

void
norm_glGetUniformiv (GLuint program, GLint location, GLint* params)
{
}

void
norm_glGetVertexAttribPointerv (GLuint index, GLenum pname, GLvoid** pointer)
{
}

void
norm_glGetVertexAttribfv (GLuint index, GLenum pname, GLfloat* params)
{
}

void
norm_glGetVertexAttribiv (GLuint index, GLenum pname, GLint* params)
{
}

GLboolean
norm_glIsBuffer (GLuint buffer)
{
	return 0;
}

GLboolean
norm_glIsFramebuffer (GLuint framebuffer)
{
	return 0;
}

GLboolean
norm_glIsProgram (GLuint program)
{
	return 0;
}

GLboolean
norm_glIsRenderbuffer (GLuint renderbuffer)
{
	return 0;
}

GLboolean
norm_glIsShader (GLuint shader)
{
	return 0;
}

void
norm_glLinkProgram (GLuint program)
{
}

void
norm_glReleaseShaderCompiler (void)
{
}

void
norm_glRenderbufferStorage (GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
}

void
norm_glSampleCoverage (GLclampf value, GLboolean invert)
{
}

void
norm_glShaderBinary (GLsizei n, const GLuint* shaders, GLenum binaryformat, const GLvoid* binary, GLsizei length)
{
}

void
norm_glShaderSource (GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
{
}

void
norm_glStencilFuncSeparate (GLenum face, GLenum func, GLint ref, GLuint mask)
{
}

void
norm_glStencilMaskSeparate (GLenum face, GLuint mask)
{
}

void
norm_glStencilOpSeparate (GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
}

void
norm_glUniform1f (GLint location, GLfloat x)
{
}

void
norm_glUniform1fv (GLint location, GLsizei count, const GLfloat* v)
{
}

void
norm_glUniform1i (GLint location, GLint x)
{
}

void
norm_glUniform1iv (GLint location, GLsizei count, const GLint* v)
{
}

void
norm_glUniform2f (GLint location, GLfloat x, GLfloat y)
{
}

void
norm_glUniform2fv (GLint location, GLsizei count, const GLfloat* v)
{
}

void
norm_glUniform2i (GLint location, GLint x, GLint y)
{
}

void
norm_glUniform2iv (GLint location, GLsizei count, const GLint* v)
{
}

void
norm_glUniform3f (GLint location, GLfloat x, GLfloat y, GLfloat z)
{
}

void
norm_glUniform3fv (GLint location, GLsizei count, const GLfloat* v)
{
}

void
norm_glUniform3i (GLint location, GLint x, GLint y, GLint z)
{
}

void
norm_glUniform3iv (GLint location, GLsizei count, const GLint* v)
{
}

void
norm_glUniform4f (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
}

void
norm_glUniform4fv (GLint location, GLsizei count, const GLfloat* v)
{
}

void
norm_glUniform4i (GLint location, GLint x, GLint y, GLint z, GLint w)
{
}

void
norm_glUniform4iv (GLint location, GLsizei count, const GLint* v)
{
}

void
norm_glUniformMatrix2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
}

void
norm_glUniformMatrix3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
}

void
norm_glUniformMatrix4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
}

void
norm_glUseProgram (GLuint program)
{
}

void
norm_glValidateProgram (GLuint program)
{
}

void
norm_glVertexAttrib1f (GLuint indx, GLfloat x)
{
}

void
norm_glVertexAttrib1fv (GLuint indx, const GLfloat* values)
{
}

void
norm_glVertexAttrib2f (GLuint indx, GLfloat x, GLfloat y)
{
}

void
norm_glVertexAttrib2fv (GLuint indx, const GLfloat* values)
{
}

void
norm_glVertexAttrib3f (GLuint indx, GLfloat x, GLfloat y, GLfloat z)
{
}

void
norm_glVertexAttrib3fv (GLuint indx, const GLfloat* values)
{
}

void
norm_glVertexAttrib4f (GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
}

void
norm_glVertexAttrib4fv (GLuint indx, const GLfloat* values)
{
}

void
norm_glVertexAttribPointer (GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr)
{
}

void
norm_glClear (GLbitfield mask)
{
}

void
norm_glClearAccum (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
}

void
norm_glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
}

void
norm_glClearDepth (GLclampd depth)
{
}

void
norm_glClearIndex (GLfloat c)
{
}

void
norm_glClearStencil (GLint s)
{
}

void
norm_glClipPlane (GLenum plane, const GLdouble * equation)
{
}

void
norm_glColor3b (GLbyte red, GLbyte green, GLbyte blue)
{
}

void
norm_glColor3bv (const GLbyte * v)
{
}

void
norm_glColor3d (GLdouble red, GLdouble green, GLdouble blue)
{
}

void
norm_glColor3dv (const GLdouble * v)
{
}

void
norm_glColor3f (GLfloat red, GLfloat green, GLfloat blue)
{
}

void
norm_glColor3fv (const GLfloat * v)
{
}

void
norm_glColor3i (GLint red, GLint green, GLint blue)
{
}

void
norm_glColor3iv (const GLint * v)
{
}

void
norm_glColor3s (GLshort red, GLshort green, GLshort blue)
{
}

void
norm_glColor3sv (const GLshort * v)
{
}

void
norm_glColor3ub (GLubyte red, GLubyte green, GLubyte blue)
{
}

void
norm_glColor3ubv (const GLubyte * v)
{
}

void
norm_glColor3ui (GLuint red, GLuint green, GLuint blue)
{
}

void
norm_glColor3uiv (const GLuint * v)
{
}

void
norm_glColor3us (GLushort red, GLushort green, GLushort blue)
{
}

void
norm_glColor3usv (const GLushort * v)
{
}

void
norm_glColor4b (GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
}

void
norm_glColor4bv (const GLbyte * v)
{
}

void
norm_glColor4d (GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
}

void
norm_glColor4dv (const GLdouble * v)
{
}

void
norm_glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
}

void
norm_glColor4fv (const GLfloat * v)
{
}

void
norm_glColor4i (GLint red, GLint green, GLint blue, GLint alpha)
{
}

void
norm_glColor4iv (const GLint * v)
{
}

void
norm_glColor4s (GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
}

void
norm_glColor4sv (const GLshort * v)
{
}

void
norm_glColor4ub (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
}

void
norm_glColor4ubv (const GLubyte * v)
{
}

void
norm_glColor4ui (GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
}

void
norm_glColor4uiv (const GLuint * v)
{
}

void
norm_glColor4us (GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
}

void
norm_glColor4usv (const GLushort * v)
{
}

void
norm_glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
}

void
norm_glColorMaterial (GLenum face, GLenum mode)
{
}

void
norm_glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr)
{
}

void
norm_glColorSubTable (GLenum target, GLsizei start, GLsizei count, GLenum format,
				 GLenum type, const GLvoid * data)
{
}

void
norm_glColorTable (GLenum target, GLenum internalformat, GLsizei width,
			  GLenum format, GLenum type, const GLvoid * table)
{
}

void
norm_glColorTableParameterfv (GLenum target, GLenum pname, const GLfloat * params)
{
}

void
norm_glColorTableParameteriv (GLenum target, GLenum pname, const GLint * params)
{
}

void
norm_glConvolutionFilter1D (GLenum target, GLenum internalformat, GLsizei width,
					   GLenum format, GLenum type, const GLvoid * image)
{
}

void
norm_glConvolutionFilter2D (GLenum target, GLenum internalformat, GLsizei width,
					   GLsizei height, GLenum format, GLenum type,
					   const GLvoid * image)
{
}

void
norm_glConvolutionParameterf (GLenum target, GLenum pname, GLfloat params)
{
}

void
norm_glConvolutionParameterfv (GLenum target, GLenum pname, const GLfloat * params)
{
}

void
norm_glConvolutionParameteri (GLenum target, GLenum pname, GLint params)
{
}

void
norm_glConvolutionParameteriv (GLenum target, GLenum pname, const GLint * params)
{
}

void
norm_glCopyColorSubTable (GLenum target, GLsizei start, GLint x, GLint y,
					 GLsizei width)
{
}

void
norm_glCopyColorTable (GLenum target, GLenum internalformat, GLint x, GLint y,
				  GLsizei width)
{
}

void
norm_glCopyConvolutionFilter1D (GLenum target, GLenum internalformat, GLint x,
						   GLint y, GLsizei width)
{
}

void
norm_glCopyConvolutionFilter2D (GLenum target, GLenum internalformat, GLint x,
						   GLint y, GLsizei width, GLsizei height)
{
}

void
norm_glCopyPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
}

void
norm_glCopyTexImage1D (GLenum target, GLint level, GLenum internalformat, GLint x,
				  GLint y, GLsizei width, GLint border)
{
}

void
norm_glCopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x,
				  GLint y, GLsizei width, GLsizei height, GLint border)
{
}

void
norm_glCopyTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLint x,
					 GLint y, GLsizei width)
{
}

void
norm_glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
					 GLint x, GLint y, GLsizei width, GLsizei height)
{
}

void
norm_glCopyTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
					 GLint zoffset, GLint x, GLint y, GLsizei width,
					 GLsizei height)
{
}

void
norm_glCullFace (GLenum mode)
{
}

void
norm_glDeleteLists (GLuint list, GLsizei range)
{
}

void
norm_glDeleteTextures (GLsizei n, const GLuint * textures)
{
}

void
norm_glDepthFunc (GLenum func)
{
}

void
norm_glDepthMask (GLboolean flag)
{
}

void
norm_glDepthRange (GLclampd near_val, GLclampd far_val)
{
}

void
norm_glDisable (GLenum cap)
{
}

void
norm_glDisableClientState (GLenum cap)
{
}

void
norm_glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
}

void
norm_glDrawBuffer (GLenum mode)
{
}

void
norm_glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid * indices)
{
}

void
norm_glDrawPixels (GLsizei width, GLsizei height, GLenum format, GLenum type,
			  const GLvoid * pixels)
{
}

void
norm_glDrawRangeElements (GLenum mode, GLuint start, GLuint end, GLsizei count,
					 GLenum type, const GLvoid * indices)
{
}

void
norm_glEdgeFlag (GLboolean flag)
{
}

void
norm_glEdgeFlagPointer (GLsizei stride, const GLvoid * ptr)
{
}

void
norm_glEdgeFlagv (const GLboolean * flag)
{
}

void
norm_glEnable (GLenum cap)
{
}

void
norm_glEnableClientState (GLenum cap)
{
}

void
norm_glEnd (void)
{
}

void
norm_glEndList (void)
{
}

void
norm_glEvalCoord1d (GLdouble u)
{
}

void
norm_glEvalCoord1dv (const GLdouble * u)
{
}

void
norm_glEvalCoord1f (GLfloat u)
{
}

void
norm_glEvalCoord1fv (const GLfloat * u)
{
}

void
norm_glEvalCoord2d (GLdouble u, GLdouble v)
{
}

void
norm_glEvalCoord2dv (const GLdouble * u)
{
}

void
norm_glEvalCoord2f (GLfloat u, GLfloat v)
{
}

void
norm_glEvalCoord2fv (const GLfloat * u)
{
}

void
norm_glEvalMesh1 (GLenum mode, GLint i1, GLint i2)
{
}

void
norm_glEvalMesh2 (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
}

void
norm_glEvalPoint1 (GLint i)
{
}

void
norm_glEvalPoint2 (GLint i, GLint j)
{
}

void
norm_glFeedbackBuffer (GLsizei size, GLenum type, GLfloat * buffer)
{
}

void
norm_glFinish (void)
{
}

void
norm_glFlush (void)
{
}

void
norm_glFogf (GLenum pname, GLfloat param)
{
}

void
norm_glFogfv (GLenum pname, const GLfloat * params)
{
}

void
norm_glFogi (GLenum pname, GLint param)
{
}

void
norm_glFogiv (GLenum pname, const GLint * params)
{
}

void
norm_glFrontFace (GLenum mode)
{
}

void
norm_glFrustum (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
		   GLdouble near_val, GLdouble far_val)
{
}

GLuint
norm_glGenLists (GLsizei range)
{
	return 0;
}

void
norm_glGenTextures (GLsizei n, GLuint * textures)
{
}

void
norm_glGetBooleanv (GLenum pname, GLboolean * params)
{
}

void
norm_glGetClipPlane (GLenum plane, GLdouble * equation)
{
}

void
norm_glGetColorTable (GLenum target, GLenum format, GLenum type, GLvoid * table)
{
}

void
norm_glGetColorTableParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
norm_glGetColorTableParameteriv (GLenum target, GLenum pname, GLint * params)
{
}

void
norm_glGetConvolutionFilter (GLenum target, GLenum format, GLenum type,
						GLvoid * image)
{
}

void
norm_glGetConvolutionParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
norm_glGetConvolutionParameteriv (GLenum target, GLenum pname, GLint * params)
{
}

void
norm_glGetDoublev (GLenum pname, GLdouble * params)
{
}

GLenum
norm_glGetError (void)
{
	return 0;
}

void
norm_glGetFloatv (GLenum pname, GLfloat * params)
{
}

void
norm_glGetHistogram (GLenum target, GLboolean reset, GLenum format, GLenum type,
				GLvoid * values)
{
}

void
norm_glGetHistogramParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
norm_glGetHistogramParameteriv (GLenum target, GLenum pname, GLint * params)
{
}

void
norm_glGetIntegerv (GLenum pname, GLint * params)
{
	switch (pname) {
		case GL_MAX_TEXTURE_SIZE:
			*params = 512;
			break;
		case GL_UNPACK_ALIGNMENT:
		case GL_PACK_ALIGNMENT:
			*params = 4;
			break;
		case GL_UNPACK_ROW_LENGTH:
		case GL_UNPACK_SKIP_ROWS:
		case GL_UNPACK_SKIP_PIXELS:
		case GL_PACK_ROW_LENGTH:
		case GL_PACK_SKIP_ROWS:
		case GL_PACK_SKIP_PIXELS:
			*params = 0;
			break;

		default:
			*params = 0;
			break;
	}
}

void
norm_glGetLightfv (GLenum light, GLenum pname, GLfloat * params)
{
}

void
norm_glGetLightiv (GLenum light, GLenum pname, GLint * params)
{
}

void
norm_glGetMapdv (GLenum target, GLenum query, GLdouble * v)
{
}

void
norm_glGetMapfv (GLenum target, GLenum query, GLfloat * v)
{
}

void
norm_glGetMapiv (GLenum target, GLenum query, GLint * v)
{
}

void
norm_glGetMaterialfv (GLenum face, GLenum pname, GLfloat * params)
{
}

void
norm_glGetMaterialiv (GLenum face, GLenum pname, GLint * params)
{
}

void
norm_glGetMinmax (GLenum target, GLboolean reset, GLenum format, GLenum types,
			 GLvoid * values)
{
}

void
norm_glGetMinmaxParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
norm_glGetMinmaxParameteriv (GLenum target, GLenum pname, GLint * params)
{
}

void
norm_glGetPixelMapfv (GLenum map, GLfloat * values)
{
}

void
norm_glGetPixelMapuiv (GLenum map, GLuint * values)
{
}

void
norm_glGetPixelMapusv (GLenum map, GLushort * values)
{
}

void
norm_glGetPointerv (GLenum pname, void **params)
{
}

void
norm_glGetPolygonStipple (GLubyte * mask)
{
}

void
norm_glGetSeparableFilter (GLenum target, GLenum format, GLenum type, GLvoid * row,
					  GLvoid * column, GLvoid * span)
{
}

const GLubyte *
norm_glGetString (GLenum name)
{
	if (name == GL_VERSION)
		return (const GLubyte *) "1.4";
	return (const GLubyte *) "";
}

void
norm_glGetTexEnvfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
norm_glGetTexEnviv (GLenum target, GLenum pname, GLint * params)
{
}

void
norm_glGetTexGendv (GLenum coord, GLenum pname, GLdouble * params)
{
}

void
norm_glGetTexGenfv (GLenum coord, GLenum pname, GLfloat * params)
{
}

void
norm_glGetTexGeniv (GLenum coord, GLenum pname, GLint * params)
{
}

void
norm_glGetTexImage (GLenum target, GLint level, GLenum format, GLenum type,
			   GLvoid * pixels)
{
}

void
norm_glGetTexLevelParameterfv (GLenum target, GLint level, GLenum pname,
						  GLfloat * params)
{
}

void
norm_glGetTexLevelParameteriv (GLenum target, GLint level, GLenum pname,
						  GLint * params)
{
}

void
norm_glGetTexParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
norm_glGetTexParameteriv (GLenum target, GLenum pname, GLint * params)
{
}

void
norm_glHint (GLenum target, GLenum mode)
{
}

void
norm_glHistogram (GLenum target, GLsizei width, GLenum internalformat,
			 GLboolean sink)
{
}

void
norm_glIndexMask (GLuint mask)
{
}

void
norm_glIndexPointer (GLenum type, GLsizei stride, const GLvoid * ptr)
{
}

void
norm_glIndexd (GLdouble c)
{
}

void
norm_glIndexdv (const GLdouble * c)
{
}

void
norm_glIndexf (GLfloat c)
{
}

void
norm_glIndexfv (const GLfloat * c)
{
}

void
norm_glIndexi (GLint c)
{
}

void
norm_glIndexiv (const GLint * c)
{
}

void
norm_glIndexs (GLshort c)
{
}

void
norm_glIndexsv (const GLshort * c)
{
}

void
norm_glIndexub (GLubyte c)
{
}

void
norm_glIndexubv (const GLubyte * c)
{
}

void
norm_glInitNames (void)
{
}

void
norm_glInterleavedArrays (GLenum format, GLsizei stride, const GLvoid * pointer)
{
}

GLboolean
norm_glIsEnabled (GLenum cap)
{
	return 0;
}

GLboolean
norm_glIsList (GLuint list)
{
	return 0;
}

GLboolean
norm_glIsTexture (GLuint texture)
{
	return 0;
}

void
norm_glLightModelf (GLenum pname, GLfloat param)
{
}

void
norm_glLightModelfv (GLenum pname, const GLfloat * params)
{
}

void
norm_glLightModeli (GLenum pname, GLint param)
{
}

void
norm_glLightModeliv (GLenum pname, const GLint * params)
{
}

void
norm_glLightf (GLenum light, GLenum pname, GLfloat param)
{
}

void
norm_glLightfv (GLenum light, GLenum pname, const GLfloat * params)
{
}

void
norm_glLighti (GLenum light, GLenum pname, GLint param)
{
}

void
norm_glLightiv (GLenum light, GLenum pname, const GLint * params)
{
}

void
norm_glLineStipple (GLint factor, GLushort pattern)
{
}

void
norm_glLineWidth (GLfloat width)
{
}

void
norm_glListBase (GLuint base)
{
}

void
norm_glLoadIdentity (void)
{
}

void
norm_glLoadMatrixd (const GLdouble * m)
{
}

void
norm_glLoadMatrixf (const GLfloat * m)
{
}

void
norm_glLoadName (GLuint name)
{
}

void
norm_glLogicOp (GLenum opcode)
{
}

void
norm_glMap1d (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order,
		 const GLdouble * points)
{
}

void
norm_glMap1f (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order,
		 const GLfloat * points)
{
}

void
norm_glMap2d (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder,
		 GLdouble v1, GLdouble v2, GLint vstride, GLint vorder,
		 const GLdouble * points)
{
}

void
norm_glMap2f (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder,
		 GLfloat v1, GLfloat v2, GLint vstride, GLint vorder,
		 const GLfloat * points)
{
}

void
norm_glMapGrid1d (GLint un, GLdouble u1, GLdouble u2)
{
}

void
norm_glMapGrid1f (GLint un, GLfloat u1, GLfloat u2)
{
}

void
norm_glMapGrid2d (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1,
			 GLdouble v2)
{
}

void
norm_glMapGrid2f (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
}

void
norm_glMaterialf (GLenum face, GLenum pname, GLfloat param)
{
}

void
norm_glMaterialfv (GLenum face, GLenum pname, const GLfloat * params)
{
}

void
norm_glMateriali (GLenum face, GLenum pname, GLint param)
{
}

void
norm_glMaterialiv (GLenum face, GLenum pname, const GLint * params)
{
}

void
norm_glMatrixMode (GLenum mode)
{
}

void
norm_glMinmax (GLenum target, GLenum internalformat, GLboolean sink)
{
}

void
norm_glMultMatrixd (const GLdouble * m)
{
}

void
norm_glMultMatrixf (const GLfloat * m)
{
}

void
norm_glNewList (GLuint list, GLenum mode)
{
}

void
norm_glNormal3b (GLbyte nx, GLbyte ny, GLbyte nz)
{
}

void
norm_glNormal3bv (const GLbyte * v)
{
}

void
norm_glNormal3d (GLdouble nx, GLdouble ny, GLdouble nz)
{
}

void
norm_glNormal3dv (const GLdouble * v)
{
}

void
norm_glNormal3f (GLfloat nx, GLfloat ny, GLfloat nz)
{
}

void
norm_glNormal3fv (const GLfloat * v)
{
}

void
norm_glNormal3i (GLint nx, GLint ny, GLint nz)
{
}

void
norm_glNormal3iv (const GLint * v)
{
}

void
norm_glNormal3s (GLshort nx, GLshort ny, GLshort nz)
{
}

void
norm_glNormal3sv (const GLshort * v)
{
}

void
norm_glNormalPointer (GLenum type, GLsizei stride, const GLvoid * ptr)
{
}

void
norm_glOrtho (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
		 GLdouble near_val, GLdouble far_val)
{
}

void
norm_glPassThrough (GLfloat token)
{
}

void
norm_glPixelMapfv (GLenum map, GLint mapsize, const GLfloat * values)
{
}

void
norm_glPixelMapuiv (GLenum map, GLint mapsize, const GLuint * values)
{
}

void
norm_glPixelMapusv (GLenum map, GLint mapsize, const GLushort * values)
{
}

void
norm_glPixelStoref (GLenum pname, GLfloat param)
{
}

void
norm_glPixelStorei (GLenum pname, GLint param)
{
}

void
norm_glPixelTransferf (GLenum pname, GLfloat param)
{
}

void
norm_glPixelTransferi (GLenum pname, GLint param)
{
}

void
norm_glPixelZoom (GLfloat xfactor, GLfloat yfactor)
{
}

void
norm_glPointSize (GLfloat size)
{
}

void
norm_glPolygonMode (GLenum face, GLenum mode)
{
}

void
norm_glPolygonOffset (GLfloat factor, GLfloat units)
{
}

void
norm_glPolygonStipple (const GLubyte * mask)
{
}

void
norm_glPopAttrib (void)
{
}

void
norm_glPopClientAttrib (void)
{
}

void
norm_glPopMatrix (void)
{
}

void
norm_glPopName (void)
{
}

void
norm_glPrioritizeTextures (GLsizei n, const GLuint * textures,
					  const GLclampf * priorities)
{
}

void
norm_glPushAttrib (GLbitfield mask)
{
}

void
norm_glPushClientAttrib (GLbitfield mask)
{
}

void
norm_glPushMatrix (void)
{
}

void
norm_glPushName (GLuint name)
{
}

void
norm_glRasterPos2d (GLdouble x, GLdouble y)
{
}

void
norm_glRasterPos2dv (const GLdouble * v)
{
}

void
norm_glRasterPos2f (GLfloat x, GLfloat y)
{
}

void
norm_glRasterPos2fv (const GLfloat * v)
{
}

void
norm_glRasterPos2i (GLint x, GLint y)
{
}

void
norm_glRasterPos2iv (const GLint * v)
{
}

void
norm_glRasterPos2s (GLshort x, GLshort y)
{
}

void
norm_glRasterPos2sv (const GLshort * v)
{
}

void
norm_glRasterPos3d (GLdouble x, GLdouble y, GLdouble z)
{
}

void
norm_glRasterPos3dv (const GLdouble * v)
{
}

void
norm_glRasterPos3f (GLfloat x, GLfloat y, GLfloat z)
{
}

void
norm_glRasterPos3fv (const GLfloat * v)
{
}

void
norm_glRasterPos3i (GLint x, GLint y, GLint z)
{
}

void
norm_glRasterPos3iv (const GLint * v)
{
}

void
norm_glRasterPos3s (GLshort x, GLshort y, GLshort z)
{
}

void
norm_glRasterPos3sv (const GLshort * v)
{
}

void
norm_glRasterPos4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
}

void
norm_glRasterPos4dv (const GLdouble * v)
{
}

void
norm_glRasterPos4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
}

void
norm_glRasterPos4fv (const GLfloat * v)
{
}

void
norm_glRasterPos4i (GLint x, GLint y, GLint z, GLint w)
{
}

void
norm_glRasterPos4iv (const GLint * v)
{
}

void
norm_glRasterPos4s (GLshort x, GLshort y, GLshort z, GLshort w)
{
}

void
norm_glRasterPos4sv (const GLshort * v)
{
}

void
norm_glReadBuffer (GLenum mode)
{
}

void
norm_glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
			  GLenum type, GLvoid * pixels)
{
}

void
norm_glRectd (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
}

void
norm_glRectdv (const GLdouble * v1, const GLdouble * v2)
{
}

void
norm_glRectf (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
}

void
norm_glRectfv (const GLfloat * v1, const GLfloat * v2)
{
}

void
norm_glRecti (GLint x1, GLint y1, GLint x2, GLint y2)
{
}

void
norm_glRectiv (const GLint * v1, const GLint * v2)
{
}

void
norm_glRects (GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
}

void
norm_glRectsv (const GLshort * v1, const GLshort * v2)
{
}

GLint
norm_glRenderMode (GLenum mode)
{
	return 0;
}

void
norm_glResetHistogram (GLenum target)
{
}

void
norm_glResetMinmax (GLenum target)
{
}

void
norm_glRotated (GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
}

void
norm_glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
}

void
norm_glScaled (GLdouble x, GLdouble y, GLdouble z)
{
}

void
norm_glScalef (GLfloat x, GLfloat y, GLfloat z)
{
}

void
norm_glScissor (GLint x, GLint y, GLsizei width, GLsizei height)
{
}

void
norm_glSelectBuffer (GLsizei size, GLuint * buffer)
{
}

void
norm_glSeparableFilter2D (GLenum target, GLenum internalformat, GLsizei width,
					 GLsizei height, GLenum format, GLenum type,
					 const GLvoid * row, const GLvoid * column)
{
}

void
norm_glShadeModel (GLenum mode)
{
}

void
norm_glStencilFunc (GLenum func, GLint ref, GLuint mask)
{
}

void
norm_glStencilMask (GLuint mask)
{
}

void
norm_glStencilOp (GLenum fail, GLenum zfail, GLenum zpass)
{
}

void
norm_glTexCoord1d (GLdouble s)
{
}

void
norm_glTexCoord1dv (const GLdouble * v)
{
}

void
norm_glTexCoord1f (GLfloat s)
{
}

void
norm_glTexCoord1fv (const GLfloat * v)
{
}

void
norm_glTexCoord1i (GLint s)
{
}

void
norm_glTexCoord1iv (const GLint * v)
{
}

void
norm_glTexCoord1s (GLshort s)
{
}

void
norm_glTexCoord1sv (const GLshort * v)
{
}

void
norm_glTexCoord2d (GLdouble s, GLdouble t)
{
}

void
norm_glTexCoord2dv (const GLdouble * v)
{
}

void
norm_glTexCoord2f (GLfloat s, GLfloat t)
{
}

void
norm_glTexCoord2fv (const GLfloat * v)
{
}

void
norm_glTexCoord2i (GLint s, GLint t)
{
}

void
norm_glTexCoord2iv (const GLint * v)
{
}

void
norm_glTexCoord2s (GLshort s, GLshort t)
{
}

void
norm_glTexCoord2sv (const GLshort * v)
{
}

void
norm_glTexCoord3d (GLdouble s, GLdouble t, GLdouble r)
{
}

void
norm_glTexCoord3dv (const GLdouble * v)
{
}

void
norm_glTexCoord3f (GLfloat s, GLfloat t, GLfloat r)
{
}

void
norm_glTexCoord3fv (const GLfloat * v)
{
}

void
norm_glTexCoord3i (GLint s, GLint t, GLint r)
{
}

void
norm_glTexCoord3iv (const GLint * v)
{
}

void
norm_glTexCoord3s (GLshort s, GLshort t, GLshort r)
{
}

void
norm_glTexCoord3sv (const GLshort * v)
{
}

void
norm_glTexCoord4d (GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
}

void
norm_glTexCoord4dv (const GLdouble * v)
{
}

void
norm_glTexCoord4f (GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
}

void
norm_glTexCoord4fv (const GLfloat * v)
{
}

void
norm_glTexCoord4i (GLint s, GLint t, GLint r, GLint q)
{
}

void
norm_glTexCoord4iv (const GLint * v)
{
}

void
norm_glTexCoord4s (GLshort s, GLshort t, GLshort r, GLshort q)
{
}

void
norm_glTexCoord4sv (const GLshort * v)
{
}

void
norm_glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr)
{
}

void
norm_glTexEnvf (GLenum target, GLenum pname, GLfloat param)
{
}

void
norm_glTexEnvfv (GLenum target, GLenum pname, const GLfloat * params)
{
}

void
norm_glTexEnvi (GLenum target, GLenum pname, GLint param)
{
}

void
norm_glTexEnviv (GLenum target, GLenum pname, const GLint * params)
{
}

void
norm_glTexGend (GLenum coord, GLenum pname, GLdouble param)
{
}

void
norm_glTexGendv (GLenum coord, GLenum pname, const GLdouble * params)
{
}

void
norm_glTexGenf (GLenum coord, GLenum pname, GLfloat param)
{
}

void
norm_glTexGenfv (GLenum coord, GLenum pname, const GLfloat * params)
{
}

void
norm_glTexGeni (GLenum coord, GLenum pname, GLint param)
{
}

void
norm_glTexGeniv (GLenum coord, GLenum pname, const GLint * params)
{
}

void
norm_glTexImage1D (GLenum target, GLint level, GLint internalFormat, GLsizei width,
			  GLint border, GLenum format, GLenum type, const GLvoid * pixels)
{
}

void
norm_glTexImage2D (GLenum target, GLint level, GLint internalFormat, GLsizei width,
			  GLsizei height, GLint border, GLenum format, GLenum type,
			  const GLvoid * pixels)
{
}

void
norm_glTexImage3D (GLenum target, GLint level, GLint internalFormat, GLsizei width,
			  GLsizei height, GLsizei depth, GLint border, GLenum format,
			  GLenum type, const GLvoid * pixels)
{
}

void
norm_glTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
}

void
norm_glTexParameterfv (GLenum target, GLenum pname, const GLfloat * params)
{
}

void
norm_glTexParameteri (GLenum target, GLenum pname, GLint param)
{
}

void
norm_glTexParameteriv (GLenum target, GLenum pname, const GLint * params)
{
}

void
norm_glTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLsizei width,
				 GLenum format, GLenum type, const GLvoid * pixels)
{
}

void
norm_glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
				 GLsizei width, GLsizei height, GLenum format, GLenum type,
				 const GLvoid * pixels)
{
}

void
norm_glTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
				 GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
				 GLenum format, GLenum type, const GLvoid * pixels)
{
}

void
norm_glTranslated (GLdouble x, GLdouble y, GLdouble z)
{
}

void
norm_glTranslatef (GLfloat x, GLfloat y, GLfloat z)
{
}

void
norm_glVertex2d (GLdouble x, GLdouble y)
{
}

void
norm_glVertex2dv (const GLdouble * v)
{
}

void
norm_glVertex2f (GLfloat x, GLfloat y)
{
}

void
norm_glVertex2fv (const GLfloat * v)
{
}

void
norm_glVertex2i (GLint x, GLint y)
{
}

void
norm_glVertex2iv (const GLint * v)
{
}

void
norm_glVertex2s (GLshort x, GLshort y)
{
}

void
norm_glVertex2sv (const GLshort * v)
{
}

void
norm_glVertex3d (GLdouble x, GLdouble y, GLdouble z)
{
}

void
norm_glVertex3dv (const GLdouble * v)
{
}

void
norm_glVertex3f (GLfloat x, GLfloat y, GLfloat z)
{
}

void
norm_glVertex3fv (const GLfloat * v)
{
}

void
norm_glVertex3i (GLint x, GLint y, GLint z)
{
}

void
norm_glVertex3iv (const GLint * v)
{
}

void
norm_glVertex3s (GLshort x, GLshort y, GLshort z)
{
}

void
norm_glVertex3sv (const GLshort * v)
{
}

void
norm_glVertex4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
}

void
norm_glVertex4dv (const GLdouble * v)
{
}

void
norm_glVertex4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
}

void
norm_glVertex4fv (const GLfloat * v)
{
}

void
norm_glVertex4i (GLint x, GLint y, GLint z, GLint w)
{
}

void
norm_glVertex4iv (const GLint * v)
{
}

void
norm_glVertex4s (GLshort x, GLshort y, GLshort z, GLshort w)
{
}

void
norm_glVertex4sv (const GLshort * v)
{
}

void
norm_glVertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr)
{
}

void
norm_glViewport (GLint x, GLint y, GLsizei width, GLsizei height)
{
}

void
norm_glPNTrianglesiATI (GLint x, GLint y)
{
}
