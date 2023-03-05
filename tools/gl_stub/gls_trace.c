#include <string.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include "QF/GL/defines.h"
#include "QF/GL/types.h"
#include "QF/GLSL/types.h"

typedef struct __GLXcontextRec *GLXContext;
typedef XID GLXDrawable;

#define TRACE do { \
	puts (__FUNCTION__);\
} while (0)

void
trace_glAccum (GLenum op, GLfloat value)
{
	TRACE;
}

void
trace_glActiveTexture (GLenum texture)
{
	TRACE;
}

void
trace_glAlphaFunc (GLenum func, GLclampf ref)
{
	TRACE;
}

GLboolean
trace_glAreTexturesResident (GLsizei n, const GLuint * textures,
					   GLboolean * residences)
{
	TRACE;
	return 0;
}

void
trace_glArrayElement (GLint i)
{
	TRACE;
}

void
trace_glAttachShader (GLuint program, GLuint shader)
{
	TRACE;
}

void
trace_glBegin (GLenum mode)
{
	TRACE;
}

void
trace_glBindAttribLocation (GLuint program, GLuint index, const GLchar* name)
{
	TRACE;
}

void
trace_glBindBuffer (GLenum target, GLuint buffer)
{
	TRACE;
}

void
trace_glBindFramebuffer (GLenum target, GLuint framebuffer)
{
	TRACE;
}

void
trace_glBindRenderbuffer (GLenum target, GLuint renderbuffer)
{
	TRACE;
}

void
trace_glBindTexture (GLenum target, GLuint texture)
{
	TRACE;
}

void
trace_glBitmap (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig,
		  GLfloat xmove, GLfloat ymove, const GLubyte * bitmap)
{
	TRACE;
}

void
trace_glBlendColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	TRACE;
}

void
trace_glBlendEquation (GLenum mode)
{
	TRACE;
}

void
trace_glBlendEquationSeparate (GLenum modeRGB, GLenum modeAlpha)
{
	TRACE;
}

void
trace_glBlendFunc (GLenum sfactor, GLenum dfactor)
{
	TRACE;
}

void
trace_glBlendFuncSeparate (GLenum sfactor, GLenum dfactor)
{
	TRACE;
}

void
trace_glBufferData (GLenum target, GLsizeiptr size, const GLvoid* data, GLenum usage)
{
	TRACE;
}

void
trace_glBufferSubData (GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid* data)
{
	TRACE;
}

void
trace_glCallList (GLuint list)
{
	TRACE;
}

void
trace_glCallLists (GLsizei n, GLenum type, const GLvoid * lists)
{
	TRACE;
}

GLenum
trace_glCheckFramebufferStatus (GLenum target)
{
	TRACE;
	return 0x8CD5;
}

void
trace_glClearDepthf (GLclampf depth)
{
	TRACE;
}

void
trace_glCompileShader (GLuint shader)
{
	TRACE;
}

void
trace_glCompressedTexImage2D (GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid* data)
{
	TRACE;
}

void
trace_glCompressedTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid* data)
{
	TRACE;
}

GLuint
trace_glCreateProgram (void)
{
	static int program;
	TRACE;
	return ++program;
}

GLuint
trace_glCreateShader (GLenum type)
{
	static int shader;
	TRACE;
	return ++shader;
}

void
trace_glDeleteBuffers (GLsizei n, const GLuint* buffers)
{
	TRACE;
}

void
trace_glDeleteFramebuffers (GLsizei n, const GLuint* framebuffers)
{
	TRACE;
}

void
trace_glDeleteProgram (GLuint program)
{
	TRACE;
}

void
trace_glDeleteRenderbuffers (GLsizei n, const GLuint* renderbuffers)
{
	TRACE;
}

void
trace_glDeleteShader (GLuint shader)
{
	TRACE;
}

void
trace_glDepthRangef (GLclampf zNear, GLclampf zFar)
{
	TRACE;
}

void
trace_glDetachShader (GLuint program, GLuint shader)
{
	TRACE;
}

void
trace_glDisableVertexAttribArray (GLuint index)
{
	TRACE;
}

void
trace_glEnableVertexAttribArray (GLuint index)
{
	TRACE;
}

void
trace_glFramebufferRenderbuffer (GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
	TRACE;
}

void
trace_glGenBuffers (GLsizei n, GLuint* buffers)
{
	TRACE;
	memset (buffers, 0, n * sizeof (GLuint));
}

void
trace_glGenFramebuffers (GLsizei n, GLuint* framebuffers)
{
	TRACE;
	memset (framebuffers, 0, n * sizeof (GLuint));
}

void
trace_glGenRenderbuffers (GLsizei n, GLuint* renderbuffers)
{
	TRACE;
	memset (renderbuffers, 0, n * sizeof (GLuint));
}

void
trace_glGenerateMipmap (GLenum target)
{
	TRACE;
}

void
trace_glGetActiveAttrib (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
	TRACE;
}

void
trace_glGetActiveUniform (GLuint program, GLuint index, GLsizei bufsize, GLsizei* length, GLint* size, GLenum* type, GLchar* name)
{
	TRACE;
}

void
trace_glGetAttachedShaders (GLuint program, GLsizei maxcount, GLsizei* count, GLuint* shaders)
{
	TRACE;
}

int
trace_glGetAttribLocation (GLuint program, const GLchar* name)
{
	TRACE;
	return 0;
}

void
trace_glGetBufferParameteriv (GLenum target, GLenum pname, GLint* params)
{
	TRACE;
}

void
trace_glGetFramebufferAttachmentParameteriv (GLenum target, GLenum attachment, GLenum pname, GLint* params)
{
	TRACE;
}

void
trace_glGetProgramInfoLog (GLuint program, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
	TRACE;
	if (bufsize > 0) {
		*infolog = 0;
	}
}

void
trace_glGetProgramiv (GLuint program, GLenum pname, GLint* params)
{
	TRACE;
	*params = 0;
}

void
trace_glGetRenderbufferParameteriv (GLuint shader, GLenum pname, GLint* params)
{
	TRACE;
}

void
trace_glGetShaderInfoLog (GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* infolog)
{
	TRACE;
	if (bufsize > 0) {
		*infolog = 0;
	}
}

void
trace_glGetShaderPrecisionFormat (GLenum shadertype, GLenum precisiontype, GLint* range, GLint* precision)
{
	TRACE;
}

void
trace_glGetShaderSource (GLuint shader, GLsizei bufsize, GLsizei* length, GLchar* source)
{
	TRACE;
}

void
trace_glGetShaderiv (GLuint shader, GLenum pname, GLint* params)
{
	TRACE;
	*params = 0;
}

int
trace_glGetUniformLocation (GLuint program, const GLchar* name)
{
	TRACE;
	return 0;
}

void
trace_glGetUniformfv (GLuint program, GLint location, GLfloat* params)
{
	TRACE;
}

void
trace_glGetUniformiv (GLuint program, GLint location, GLint* params)
{
	TRACE;
}

void
trace_glGetVertexAttribPointerv (GLuint index, GLenum pname, GLvoid** pointer)
{
	TRACE;
}

void
trace_glGetVertexAttribfv (GLuint index, GLenum pname, GLfloat* params)
{
	TRACE;
}

void
trace_glGetVertexAttribiv (GLuint index, GLenum pname, GLint* params)
{
	TRACE;
}

GLboolean
trace_glIsBuffer (GLuint buffer)
{
	TRACE;
	return 0;
}

GLboolean
trace_glIsFramebuffer (GLuint framebuffer)
{
	TRACE;
	return 0;
}

GLboolean
trace_glIsProgram (GLuint program)
{
	TRACE;
	return 0;
}

GLboolean
trace_glIsRenderbuffer (GLuint renderbuffer)
{
	TRACE;
	return 0;
}

GLboolean
trace_glIsShader (GLuint shader)
{
	TRACE;
	return 0;
}

void
trace_glLinkProgram (GLuint program)
{
	TRACE;
}

void
trace_glReleaseShaderCompiler (void)
{
	TRACE;
}

void
trace_glRenderbufferStorage (GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
	TRACE;
}

void
trace_glSampleCoverage (GLclampf value, GLboolean invert)
{
	TRACE;
}

void
trace_glShaderBinary (GLsizei n, const GLuint* shaders, GLenum binaryformat, const GLvoid* binary, GLsizei length)
{
	TRACE;
}

void
trace_glShaderSource (GLuint shader, GLsizei count, const GLchar** string, const GLint* length)
{
	TRACE;
}

void
trace_glStencilFuncSeparate (GLenum face, GLenum func, GLint ref, GLuint mask)
{
	TRACE;
}

void
trace_glStencilMaskSeparate (GLenum face, GLuint mask)
{
	TRACE;
}

void
trace_glStencilOpSeparate (GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
	TRACE;
}

void
trace_glUniform1f (GLint location, GLfloat x)
{
	TRACE;
}

void
trace_glUniform1fv (GLint location, GLsizei count, const GLfloat* v)
{
	TRACE;
}

void
trace_glUniform1i (GLint location, GLint x)
{
	TRACE;
}

void
trace_glUniform1iv (GLint location, GLsizei count, const GLint* v)
{
	TRACE;
}

void
trace_glUniform2f (GLint location, GLfloat x, GLfloat y)
{
	TRACE;
}

void
trace_glUniform2fv (GLint location, GLsizei count, const GLfloat* v)
{
	TRACE;
}

void
trace_glUniform2i (GLint location, GLint x, GLint y)
{
	TRACE;
}

void
trace_glUniform2iv (GLint location, GLsizei count, const GLint* v)
{
	TRACE;
}

void
trace_glUniform3f (GLint location, GLfloat x, GLfloat y, GLfloat z)
{
	TRACE;
}

void
trace_glUniform3fv (GLint location, GLsizei count, const GLfloat* v)
{
	TRACE;
}

void
trace_glUniform3i (GLint location, GLint x, GLint y, GLint z)
{
	TRACE;
}

void
trace_glUniform3iv (GLint location, GLsizei count, const GLint* v)
{
	TRACE;
}

void
trace_glUniform4f (GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	TRACE;
}

void
trace_glUniform4fv (GLint location, GLsizei count, const GLfloat* v)
{
	TRACE;
}

void
trace_glUniform4i (GLint location, GLint x, GLint y, GLint z, GLint w)
{
	TRACE;
}

void
trace_glUniform4iv (GLint location, GLsizei count, const GLint* v)
{
	TRACE;
}

void
trace_glUniformMatrix2fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
	TRACE;
}

void
trace_glUniformMatrix3fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
	TRACE;
}

void
trace_glUniformMatrix4fv (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value)
{
	TRACE;
}

void
trace_glUseProgram (GLuint program)
{
	TRACE;
}

void
trace_glValidateProgram (GLuint program)
{
	TRACE;
}

void
trace_glVertexAttrib1f (GLuint indx, GLfloat x)
{
	TRACE;
}

void
trace_glVertexAttrib1fv (GLuint indx, const GLfloat* values)
{
	TRACE;
}

void
trace_glVertexAttrib2f (GLuint indx, GLfloat x, GLfloat y)
{
	TRACE;
}

void
trace_glVertexAttrib2fv (GLuint indx, const GLfloat* values)
{
	TRACE;
}

void
trace_glVertexAttrib3f (GLuint indx, GLfloat x, GLfloat y, GLfloat z)
{
	TRACE;
}

void
trace_glVertexAttrib3fv (GLuint indx, const GLfloat* values)
{
	TRACE;
}

void
trace_glVertexAttrib4f (GLuint indx, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	TRACE;
}

void
trace_glVertexAttrib4fv (GLuint indx, const GLfloat* values)
{
	TRACE;
}

void
trace_glVertexAttribPointer (GLuint indx, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid* ptr)
{
	TRACE;
}

void
trace_glClear (GLbitfield mask)
{
	TRACE;
}

void
trace_glClearAccum (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	TRACE;
}

void
trace_glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
	TRACE;
}

void
trace_glClearDepth (GLclampd depth)
{
	TRACE;
}

void
trace_glClearIndex (GLfloat c)
{
	TRACE;
}

void
trace_glClearStencil (GLint s)
{
	TRACE;
}

void
trace_glClipPlane (GLenum plane, const GLdouble * equation)
{
	TRACE;
}

void
trace_glColor3b (GLbyte red, GLbyte green, GLbyte blue)
{
	TRACE;
}

void
trace_glColor3bv (const GLbyte * v)
{
	TRACE;
}

void
trace_glColor3d (GLdouble red, GLdouble green, GLdouble blue)
{
	TRACE;
}

void
trace_glColor3dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glColor3f (GLfloat red, GLfloat green, GLfloat blue)
{
	TRACE;
}

void
trace_glColor3fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glColor3i (GLint red, GLint green, GLint blue)
{
	TRACE;
}

void
trace_glColor3iv (const GLint * v)
{
	TRACE;
}

void
trace_glColor3s (GLshort red, GLshort green, GLshort blue)
{
	TRACE;
}

void
trace_glColor3sv (const GLshort * v)
{
	TRACE;
}

void
trace_glColor3ub (GLubyte red, GLubyte green, GLubyte blue)
{
	TRACE;
}

void
trace_glColor3ubv (const GLubyte * v)
{
	TRACE;
}

void
trace_glColor3ui (GLuint red, GLuint green, GLuint blue)
{
	TRACE;
}

void
trace_glColor3uiv (const GLuint * v)
{
	TRACE;
}

void
trace_glColor3us (GLushort red, GLushort green, GLushort blue)
{
	TRACE;
}

void
trace_glColor3usv (const GLushort * v)
{
	TRACE;
}

void
trace_glColor4b (GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
	TRACE;
}

void
trace_glColor4bv (const GLbyte * v)
{
	TRACE;
}

void
trace_glColor4d (GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
	TRACE;
}

void
trace_glColor4dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
	TRACE;
}

void
trace_glColor4fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glColor4i (GLint red, GLint green, GLint blue, GLint alpha)
{
	TRACE;
}

void
trace_glColor4iv (const GLint * v)
{
	TRACE;
}

void
trace_glColor4s (GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
	TRACE;
}

void
trace_glColor4sv (const GLshort * v)
{
	TRACE;
}

void
trace_glColor4ub (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
	TRACE;
}

void
trace_glColor4ubv (const GLubyte * v)
{
	TRACE;
}

void
trace_glColor4ui (GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
	TRACE;
}

void
trace_glColor4uiv (const GLuint * v)
{
	TRACE;
}

void
trace_glColor4us (GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
	TRACE;
}

void
trace_glColor4usv (const GLushort * v)
{
	TRACE;
}

void
trace_glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
	TRACE;
}

void
trace_glColorMaterial (GLenum face, GLenum mode)
{
	TRACE;
}

void
trace_glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr)
{
	TRACE;
}

void
trace_glColorSubTable (GLenum target, GLsizei start, GLsizei count, GLenum format,
				 GLenum type, const GLvoid * data)
{
	TRACE;
}

void
trace_glColorTable (GLenum target, GLenum internalformat, GLsizei width,
			  GLenum format, GLenum type, const GLvoid * table)
{
	TRACE;
}

void
trace_glColorTableParameterfv (GLenum target, GLenum pname, const GLfloat * params)
{
	TRACE;
}

void
trace_glColorTableParameteriv (GLenum target, GLenum pname, const GLint * params)
{
	TRACE;
}

void
trace_glConvolutionFilter1D (GLenum target, GLenum internalformat, GLsizei width,
					   GLenum format, GLenum type, const GLvoid * image)
{
	TRACE;
}

void
trace_glConvolutionFilter2D (GLenum target, GLenum internalformat, GLsizei width,
					   GLsizei height, GLenum format, GLenum type,
					   const GLvoid * image)
{
	TRACE;
}

void
trace_glConvolutionParameterf (GLenum target, GLenum pname, GLfloat params)
{
	TRACE;
}

void
trace_glConvolutionParameterfv (GLenum target, GLenum pname, const GLfloat * params)
{
	TRACE;
}

void
trace_glConvolutionParameteri (GLenum target, GLenum pname, GLint params)
{
	TRACE;
}

void
trace_glConvolutionParameteriv (GLenum target, GLenum pname, const GLint * params)
{
	TRACE;
}

void
trace_glCopyColorSubTable (GLenum target, GLsizei start, GLint x, GLint y,
					 GLsizei width)
{
	TRACE;
}

void
trace_glCopyColorTable (GLenum target, GLenum internalformat, GLint x, GLint y,
				  GLsizei width)
{
	TRACE;
}

void
trace_glCopyConvolutionFilter1D (GLenum target, GLenum internalformat, GLint x,
						   GLint y, GLsizei width)
{
	TRACE;
}

void
trace_glCopyConvolutionFilter2D (GLenum target, GLenum internalformat, GLint x,
						   GLint y, GLsizei width, GLsizei height)
{
	TRACE;
}

void
trace_glCopyPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
	TRACE;
}

void
trace_glCopyTexImage1D (GLenum target, GLint level, GLenum internalformat, GLint x,
				  GLint y, GLsizei width, GLint border)
{
	TRACE;
}

void
trace_glCopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x,
				  GLint y, GLsizei width, GLsizei height, GLint border)
{
	TRACE;
}

void
trace_glCopyTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLint x,
					 GLint y, GLsizei width)
{
	TRACE;
}

void
trace_glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
					 GLint x, GLint y, GLsizei width, GLsizei height)
{
	TRACE;
}

void
trace_glCopyTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
					 GLint zoffset, GLint x, GLint y, GLsizei width,
					 GLsizei height)
{
	TRACE;
}

void
trace_glCullFace (GLenum mode)
{
	TRACE;
}

void
trace_glDeleteLists (GLuint list, GLsizei range)
{
	TRACE;
}

void
trace_glDeleteTextures (GLsizei n, const GLuint * textures)
{
	TRACE;
}

void
trace_glDepthFunc (GLenum func)
{
	TRACE;
}

void
trace_glDepthMask (GLboolean flag)
{
	TRACE;
}

void
trace_glDepthRange (GLclampd near_val, GLclampd far_val)
{
	TRACE;
}

void
trace_glDisable (GLenum cap)
{
	TRACE;
}

void
trace_glDisableClientState (GLenum cap)
{
	TRACE;
}

void
trace_glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
	TRACE;
}

void
trace_glDrawBuffer (GLenum mode)
{
	TRACE;
}

void
trace_glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid * indices)
{
	TRACE;
}

void
trace_glDrawPixels (GLsizei width, GLsizei height, GLenum format, GLenum type,
			  const GLvoid * pixels)
{
	TRACE;
}

void
trace_glDrawRangeElements (GLenum mode, GLuint start, GLuint end, GLsizei count,
					 GLenum type, const GLvoid * indices)
{
	TRACE;
}

void
trace_glEdgeFlag (GLboolean flag)
{
	TRACE;
}

void
trace_glEdgeFlagPointer (GLsizei stride, const GLvoid * ptr)
{
	TRACE;
}

void
trace_glEdgeFlagv (const GLboolean * flag)
{
	TRACE;
}

void
trace_glEnable (GLenum cap)
{
	TRACE;
}

void
trace_glEnableClientState (GLenum cap)
{
	TRACE;
}

void
trace_glEnd (void)
{
	TRACE;
}

void
trace_glEndList (void)
{
	TRACE;
}

void
trace_glEvalCoord1d (GLdouble u)
{
	TRACE;
}

void
trace_glEvalCoord1dv (const GLdouble * u)
{
	TRACE;
}

void
trace_glEvalCoord1f (GLfloat u)
{
	TRACE;
}

void
trace_glEvalCoord1fv (const GLfloat * u)
{
	TRACE;
}

void
trace_glEvalCoord2d (GLdouble u, GLdouble v)
{
	TRACE;
}

void
trace_glEvalCoord2dv (const GLdouble * u)
{
	TRACE;
}

void
trace_glEvalCoord2f (GLfloat u, GLfloat v)
{
	TRACE;
}

void
trace_glEvalCoord2fv (const GLfloat * u)
{
	TRACE;
}

void
trace_glEvalMesh1 (GLenum mode, GLint i1, GLint i2)
{
	TRACE;
}

void
trace_glEvalMesh2 (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
	TRACE;
}

void
trace_glEvalPoint1 (GLint i)
{
	TRACE;
}

void
trace_glEvalPoint2 (GLint i, GLint j)
{
	TRACE;
}

void
trace_glFeedbackBuffer (GLsizei size, GLenum type, GLfloat * buffer)
{
	TRACE;
}

void
trace_glFinish (void)
{
	TRACE;
}

void
trace_glFlush (void)
{
	TRACE;
}

void
trace_glFogf (GLenum pname, GLfloat param)
{
	TRACE;
}

void
trace_glFogfv (GLenum pname, const GLfloat * params)
{
	TRACE;
}

void
trace_glFogi (GLenum pname, GLint param)
{
	TRACE;
}

void
trace_glFogiv (GLenum pname, const GLint * params)
{
	TRACE;
}

void
trace_glFrontFace (GLenum mode)
{
	TRACE;
}

void
trace_glFrustum (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
		   GLdouble near_val, GLdouble far_val)
{
	TRACE;
}

GLuint
trace_glGenLists (GLsizei range)
{
	TRACE;
	return 0;
}

void
trace_glGenTextures (GLsizei n, GLuint * textures)
{
	TRACE;
	memset (textures, 0, n * sizeof (GLuint));
}

void
trace_glGetBooleanv (GLenum pname, GLboolean * params)
{
	TRACE;
}

void
trace_glGetClipPlane (GLenum plane, GLdouble * equation)
{
	TRACE;
}

void
trace_glGetColorTable (GLenum target, GLenum format, GLenum type, GLvoid * table)
{
	TRACE;
}

void
trace_glGetColorTableParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
	TRACE;
}

void
trace_glGetColorTableParameteriv (GLenum target, GLenum pname, GLint * params)
{
	TRACE;
}

void
trace_glGetConvolutionFilter (GLenum target, GLenum format, GLenum type,
						GLvoid * image)
{
	TRACE;
}

void
trace_glGetConvolutionParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
	TRACE;
}

void
trace_glGetConvolutionParameteriv (GLenum target, GLenum pname, GLint * params)
{
	TRACE;
}

void
trace_glGetDoublev (GLenum pname, GLdouble * params)
{
	TRACE;
}

GLenum
trace_glGetError (void)
{
	TRACE;
	return 0;
}

void
trace_glGetFloatv (GLenum pname, GLfloat * params)
{
	TRACE;
}

void
trace_glGetHistogram (GLenum target, GLboolean reset, GLenum format, GLenum type,
				GLvoid * values)
{
	TRACE;
}

void
trace_glGetHistogramParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
	TRACE;
}

void
trace_glGetHistogramParameteriv (GLenum target, GLenum pname, GLint * params)
{
	TRACE;
}

void
trace_glGetIntegerv (GLenum pname, GLint * params)
{
	TRACE;
	switch (pname) {
		case GL_MAX_TEXTURE_SIZE:
			*params = 2048;
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
trace_glGetLightfv (GLenum light, GLenum pname, GLfloat * params)
{
	TRACE;
}

void
trace_glGetLightiv (GLenum light, GLenum pname, GLint * params)
{
	TRACE;
}

void
trace_glGetMapdv (GLenum target, GLenum query, GLdouble * v)
{
	TRACE;
}

void
trace_glGetMapfv (GLenum target, GLenum query, GLfloat * v)
{
	TRACE;
}

void
trace_glGetMapiv (GLenum target, GLenum query, GLint * v)
{
	TRACE;
}

void
trace_glGetMaterialfv (GLenum face, GLenum pname, GLfloat * params)
{
	TRACE;
}

void
trace_glGetMaterialiv (GLenum face, GLenum pname, GLint * params)
{
	TRACE;
}

void
trace_glGetMinmax (GLenum target, GLboolean reset, GLenum format, GLenum types,
			 GLvoid * values)
{
	TRACE;
}

void
trace_glGetMinmaxParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
	TRACE;
}

void
trace_glGetMinmaxParameteriv (GLenum target, GLenum pname, GLint * params)
{
	TRACE;
}

void
trace_glGetPixelMapfv (GLenum map, GLfloat * values)
{
	TRACE;
}

void
trace_glGetPixelMapuiv (GLenum map, GLuint * values)
{
	TRACE;
}

void
trace_glGetPixelMapusv (GLenum map, GLushort * values)
{
	TRACE;
}

void
trace_glGetPointerv (GLenum pname, void **params)
{
	TRACE;
}

void
trace_glGetPolygonStipple (GLubyte * mask)
{
	TRACE;
}

void
trace_glGetSeparableFilter (GLenum target, GLenum format, GLenum type, GLvoid * row,
					  GLvoid * column, GLvoid * span)
{
	TRACE;
}

const GLubyte *
trace_glGetString (GLenum name)
{
	TRACE;
	if (name == GL_VERSION)
		return (const GLubyte *) "1.4";
	return (const GLubyte *) "";
}

void
trace_glGetTexEnvfv (GLenum target, GLenum pname, GLfloat * params)
{
	TRACE;
}

void
trace_glGetTexEnviv (GLenum target, GLenum pname, GLint * params)
{
	TRACE;
}

void
trace_glGetTexGendv (GLenum coord, GLenum pname, GLdouble * params)
{
	TRACE;
}

void
trace_glGetTexGenfv (GLenum coord, GLenum pname, GLfloat * params)
{
	TRACE;
}

void
trace_glGetTexGeniv (GLenum coord, GLenum pname, GLint * params)
{
	TRACE;
}

void
trace_glGetTexImage (GLenum target, GLint level, GLenum format, GLenum type,
			   GLvoid * pixels)
{
	TRACE;
}

void
trace_glGetTexLevelParameterfv (GLenum target, GLint level, GLenum pname,
						  GLfloat * params)
{
	TRACE;
}

void
trace_glGetTexLevelParameteriv (GLenum target, GLint level, GLenum pname,
						  GLint * params)
{
	TRACE;
}

void
trace_glGetTexParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
	TRACE;
}

void
trace_glGetTexParameteriv (GLenum target, GLenum pname, GLint * params)
{
	TRACE;
}

void
trace_glHint (GLenum target, GLenum mode)
{
	TRACE;
}

void
trace_glHistogram (GLenum target, GLsizei width, GLenum internalformat,
			 GLboolean sink)
{
	TRACE;
}

void
trace_glIndexMask (GLuint mask)
{
	TRACE;
}

void
trace_glIndexPointer (GLenum type, GLsizei stride, const GLvoid * ptr)
{
	TRACE;
}

void
trace_glIndexd (GLdouble c)
{
	TRACE;
}

void
trace_glIndexdv (const GLdouble * c)
{
	TRACE;
}

void
trace_glIndexf (GLfloat c)
{
	TRACE;
}

void
trace_glIndexfv (const GLfloat * c)
{
	TRACE;
}

void
trace_glIndexi (GLint c)
{
	TRACE;
}

void
trace_glIndexiv (const GLint * c)
{
	TRACE;
}

void
trace_glIndexs (GLshort c)
{
	TRACE;
}

void
trace_glIndexsv (const GLshort * c)
{
	TRACE;
}

void
trace_glIndexub (GLubyte c)
{
	TRACE;
}

void
trace_glIndexubv (const GLubyte * c)
{
	TRACE;
}

void
trace_glInitNames (void)
{
	TRACE;
}

void
trace_glInterleavedArrays (GLenum format, GLsizei stride, const GLvoid * pointer)
{
	TRACE;
}

GLboolean
trace_glIsEnabled (GLenum cap)
{
	TRACE;
	return 0;
}

GLboolean
trace_glIsList (GLuint list)
{
	TRACE;
	return 0;
}

GLboolean
trace_glIsTexture (GLuint texture)
{
	TRACE;
	return 0;
}

void
trace_glLightModelf (GLenum pname, GLfloat param)
{
	TRACE;
}

void
trace_glLightModelfv (GLenum pname, const GLfloat * params)
{
	TRACE;
}

void
trace_glLightModeli (GLenum pname, GLint param)
{
	TRACE;
}

void
trace_glLightModeliv (GLenum pname, const GLint * params)
{
	TRACE;
}

void
trace_glLightf (GLenum light, GLenum pname, GLfloat param)
{
	TRACE;
}

void
trace_glLightfv (GLenum light, GLenum pname, const GLfloat * params)
{
	TRACE;
}

void
trace_glLighti (GLenum light, GLenum pname, GLint param)
{
	TRACE;
}

void
trace_glLightiv (GLenum light, GLenum pname, const GLint * params)
{
	TRACE;
}

void
trace_glLineStipple (GLint factor, GLushort pattern)
{
	TRACE;
}

void
trace_glLineWidth (GLfloat width)
{
	TRACE;
}

void
trace_glListBase (GLuint base)
{
	TRACE;
}

void
trace_glLoadIdentity (void)
{
	TRACE;
}

void
trace_glLoadMatrixd (const GLdouble * m)
{
	TRACE;
}

void
trace_glLoadMatrixf (const GLfloat * m)
{
	TRACE;
}

void
trace_glLoadName (GLuint name)
{
	TRACE;
}

void
trace_glLogicOp (GLenum opcode)
{
	TRACE;
}

void
trace_glMap1d (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order,
		 const GLdouble * points)
{
	TRACE;
}

void
trace_glMap1f (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order,
		 const GLfloat * points)
{
	TRACE;
}

void
trace_glMap2d (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder,
		 GLdouble v1, GLdouble v2, GLint vstride, GLint vorder,
		 const GLdouble * points)
{
	TRACE;
}

void
trace_glMap2f (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder,
		 GLfloat v1, GLfloat v2, GLint vstride, GLint vorder,
		 const GLfloat * points)
{
	TRACE;
}

void
trace_glMapGrid1d (GLint un, GLdouble u1, GLdouble u2)
{
	TRACE;
}

void
trace_glMapGrid1f (GLint un, GLfloat u1, GLfloat u2)
{
	TRACE;
}

void
trace_glMapGrid2d (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1,
			 GLdouble v2)
{
	TRACE;
}

void
trace_glMapGrid2f (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
	TRACE;
}

void
trace_glMaterialf (GLenum face, GLenum pname, GLfloat param)
{
	TRACE;
}

void
trace_glMaterialfv (GLenum face, GLenum pname, const GLfloat * params)
{
	TRACE;
}

void
trace_glMateriali (GLenum face, GLenum pname, GLint param)
{
	TRACE;
}

void
trace_glMaterialiv (GLenum face, GLenum pname, const GLint * params)
{
	TRACE;
}

void
trace_glMatrixMode (GLenum mode)
{
	TRACE;
}

void
trace_glMinmax (GLenum target, GLenum internalformat, GLboolean sink)
{
	TRACE;
}

void
trace_glMultMatrixd (const GLdouble * m)
{
	TRACE;
}

void
trace_glMultMatrixf (const GLfloat * m)
{
	TRACE;
}

void
trace_glNewList (GLuint list, GLenum mode)
{
	TRACE;
}

void
trace_glNormal3b (GLbyte nx, GLbyte ny, GLbyte nz)
{
	TRACE;
}

void
trace_glNormal3bv (const GLbyte * v)
{
	TRACE;
}

void
trace_glNormal3d (GLdouble nx, GLdouble ny, GLdouble nz)
{
	TRACE;
}

void
trace_glNormal3dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glNormal3f (GLfloat nx, GLfloat ny, GLfloat nz)
{
	TRACE;
}

void
trace_glNormal3fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glNormal3i (GLint nx, GLint ny, GLint nz)
{
	TRACE;
}

void
trace_glNormal3iv (const GLint * v)
{
	TRACE;
}

void
trace_glNormal3s (GLshort nx, GLshort ny, GLshort nz)
{
	TRACE;
}

void
trace_glNormal3sv (const GLshort * v)
{
	TRACE;
}

void
trace_glNormalPointer (GLenum type, GLsizei stride, const GLvoid * ptr)
{
	TRACE;
}

void
trace_glOrtho (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
		 GLdouble near_val, GLdouble far_val)
{
	TRACE;
}

void
trace_glPassThrough (GLfloat token)
{
	TRACE;
}

void
trace_glPixelMapfv (GLenum map, GLint mapsize, const GLfloat * values)
{
	TRACE;
}

void
trace_glPixelMapuiv (GLenum map, GLint mapsize, const GLuint * values)
{
	TRACE;
}

void
trace_glPixelMapusv (GLenum map, GLint mapsize, const GLushort * values)
{
	TRACE;
}

void
trace_glPixelStoref (GLenum pname, GLfloat param)
{
	TRACE;
}

void
trace_glPixelStorei (GLenum pname, GLint param)
{
	TRACE;
}

void
trace_glPixelTransferf (GLenum pname, GLfloat param)
{
	TRACE;
}

void
trace_glPixelTransferi (GLenum pname, GLint param)
{
	TRACE;
}

void
trace_glPixelZoom (GLfloat xfactor, GLfloat yfactor)
{
	TRACE;
}

void
trace_glPointSize (GLfloat size)
{
	TRACE;
}

void
trace_glPolygonMode (GLenum face, GLenum mode)
{
	TRACE;
}

void
trace_glPolygonOffset (GLfloat factor, GLfloat units)
{
	TRACE;
}

void
trace_glPolygonStipple (const GLubyte * mask)
{
	TRACE;
}

void
trace_glPopAttrib (void)
{
	TRACE;
}

void
trace_glPopClientAttrib (void)
{
	TRACE;
}

void
trace_glPopMatrix (void)
{
	TRACE;
}

void
trace_glPopName (void)
{
	TRACE;
}

void
trace_glPrioritizeTextures (GLsizei n, const GLuint * textures,
					  const GLclampf * priorities)
{
	TRACE;
}

void
trace_glPushAttrib (GLbitfield mask)
{
	TRACE;
}

void
trace_glPushClientAttrib (GLbitfield mask)
{
	TRACE;
}

void
trace_glPushMatrix (void)
{
	TRACE;
}

void
trace_glPushName (GLuint name)
{
	TRACE;
}

void
trace_glRasterPos2d (GLdouble x, GLdouble y)
{
	TRACE;
}

void
trace_glRasterPos2dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glRasterPos2f (GLfloat x, GLfloat y)
{
	TRACE;
}

void
trace_glRasterPos2fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glRasterPos2i (GLint x, GLint y)
{
	TRACE;
}

void
trace_glRasterPos2iv (const GLint * v)
{
	TRACE;
}

void
trace_glRasterPos2s (GLshort x, GLshort y)
{
	TRACE;
}

void
trace_glRasterPos2sv (const GLshort * v)
{
	TRACE;
}

void
trace_glRasterPos3d (GLdouble x, GLdouble y, GLdouble z)
{
	TRACE;
}

void
trace_glRasterPos3dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glRasterPos3f (GLfloat x, GLfloat y, GLfloat z)
{
	TRACE;
}

void
trace_glRasterPos3fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glRasterPos3i (GLint x, GLint y, GLint z)
{
	TRACE;
}

void
trace_glRasterPos3iv (const GLint * v)
{
	TRACE;
}

void
trace_glRasterPos3s (GLshort x, GLshort y, GLshort z)
{
	TRACE;
}

void
trace_glRasterPos3sv (const GLshort * v)
{
	TRACE;
}

void
trace_glRasterPos4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	TRACE;
}

void
trace_glRasterPos4dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glRasterPos4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	TRACE;
}

void
trace_glRasterPos4fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glRasterPos4i (GLint x, GLint y, GLint z, GLint w)
{
	TRACE;
}

void
trace_glRasterPos4iv (const GLint * v)
{
	TRACE;
}

void
trace_glRasterPos4s (GLshort x, GLshort y, GLshort z, GLshort w)
{
	TRACE;
}

void
trace_glRasterPos4sv (const GLshort * v)
{
	TRACE;
}

void
trace_glReadBuffer (GLenum mode)
{
	TRACE;
}

void
trace_glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
			  GLenum type, GLvoid * pixels)
{
	TRACE;
}

void
trace_glRectd (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
	TRACE;
}

void
trace_glRectdv (const GLdouble * v1, const GLdouble * v2)
{
	TRACE;
}

void
trace_glRectf (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
	TRACE;
}

void
trace_glRectfv (const GLfloat * v1, const GLfloat * v2)
{
	TRACE;
}

void
trace_glRecti (GLint x1, GLint y1, GLint x2, GLint y2)
{
	TRACE;
}

void
trace_glRectiv (const GLint * v1, const GLint * v2)
{
	TRACE;
}

void
trace_glRects (GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
	TRACE;
}

void
trace_glRectsv (const GLshort * v1, const GLshort * v2)
{
	TRACE;
}

GLint
trace_glRenderMode (GLenum mode)
{
	TRACE;
	return 0;
}

void
trace_glResetHistogram (GLenum target)
{
	TRACE;
}

void
trace_glResetMinmax (GLenum target)
{
	TRACE;
}

void
trace_glRotated (GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
	TRACE;
}

void
trace_glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
	TRACE;
}

void
trace_glScaled (GLdouble x, GLdouble y, GLdouble z)
{
	TRACE;
}

void
trace_glScalef (GLfloat x, GLfloat y, GLfloat z)
{
	TRACE;
}

void
trace_glScissor (GLint x, GLint y, GLsizei width, GLsizei height)
{
	TRACE;
}

void
trace_glSelectBuffer (GLsizei size, GLuint * buffer)
{
	TRACE;
}

void
trace_glSeparableFilter2D (GLenum target, GLenum internalformat, GLsizei width,
					 GLsizei height, GLenum format, GLenum type,
					 const GLvoid * row, const GLvoid * column)
{
	TRACE;
}

void
trace_glShadeModel (GLenum mode)
{
	TRACE;
}

void
trace_glStencilFunc (GLenum func, GLint ref, GLuint mask)
{
	TRACE;
}

void
trace_glStencilMask (GLuint mask)
{
	TRACE;
}

void
trace_glStencilOp (GLenum fail, GLenum zfail, GLenum zpass)
{
	TRACE;
}

void
trace_glTexCoord1d (GLdouble s)
{
	TRACE;
}

void
trace_glTexCoord1dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glTexCoord1f (GLfloat s)
{
	TRACE;
}

void
trace_glTexCoord1fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glTexCoord1i (GLint s)
{
	TRACE;
}

void
trace_glTexCoord1iv (const GLint * v)
{
	TRACE;
}

void
trace_glTexCoord1s (GLshort s)
{
	TRACE;
}

void
trace_glTexCoord1sv (const GLshort * v)
{
	TRACE;
}

void
trace_glTexCoord2d (GLdouble s, GLdouble t)
{
	TRACE;
}

void
trace_glTexCoord2dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glTexCoord2f (GLfloat s, GLfloat t)
{
	TRACE;
}

void
trace_glTexCoord2fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glTexCoord2i (GLint s, GLint t)
{
	TRACE;
}

void
trace_glTexCoord2iv (const GLint * v)
{
	TRACE;
}

void
trace_glTexCoord2s (GLshort s, GLshort t)
{
	TRACE;
}

void
trace_glTexCoord2sv (const GLshort * v)
{
	TRACE;
}

void
trace_glTexCoord3d (GLdouble s, GLdouble t, GLdouble r)
{
	TRACE;
}

void
trace_glTexCoord3dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glTexCoord3f (GLfloat s, GLfloat t, GLfloat r)
{
	TRACE;
}

void
trace_glTexCoord3fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glTexCoord3i (GLint s, GLint t, GLint r)
{
	TRACE;
}

void
trace_glTexCoord3iv (const GLint * v)
{
	TRACE;
}

void
trace_glTexCoord3s (GLshort s, GLshort t, GLshort r)
{
	TRACE;
}

void
trace_glTexCoord3sv (const GLshort * v)
{
	TRACE;
}

void
trace_glTexCoord4d (GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
	TRACE;
}

void
trace_glTexCoord4dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glTexCoord4f (GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
	TRACE;
}

void
trace_glTexCoord4fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glTexCoord4i (GLint s, GLint t, GLint r, GLint q)
{
	TRACE;
}

void
trace_glTexCoord4iv (const GLint * v)
{
	TRACE;
}

void
trace_glTexCoord4s (GLshort s, GLshort t, GLshort r, GLshort q)
{
	TRACE;
}

void
trace_glTexCoord4sv (const GLshort * v)
{
	TRACE;
}

void
trace_glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr)
{
	TRACE;
}

void
trace_glTexEnvf (GLenum target, GLenum pname, GLfloat param)
{
	TRACE;
}

void
trace_glTexEnvfv (GLenum target, GLenum pname, const GLfloat * params)
{
	TRACE;
}

void
trace_glTexEnvi (GLenum target, GLenum pname, GLint param)
{
	TRACE;
}

void
trace_glTexEnviv (GLenum target, GLenum pname, const GLint * params)
{
	TRACE;
}

void
trace_glTexGend (GLenum coord, GLenum pname, GLdouble param)
{
	TRACE;
}

void
trace_glTexGendv (GLenum coord, GLenum pname, const GLdouble * params)
{
	TRACE;
}

void
trace_glTexGenf (GLenum coord, GLenum pname, GLfloat param)
{
	TRACE;
}

void
trace_glTexGenfv (GLenum coord, GLenum pname, const GLfloat * params)
{
	TRACE;
}

void
trace_glTexGeni (GLenum coord, GLenum pname, GLint param)
{
	TRACE;
}

void
trace_glTexGeniv (GLenum coord, GLenum pname, const GLint * params)
{
	TRACE;
}

void
trace_glTexImage1D (GLenum target, GLint level, GLint internalFormat, GLsizei width,
			  GLint border, GLenum format, GLenum type, const GLvoid * pixels)
{
	TRACE;
}

void
trace_glTexImage2D (GLenum target, GLint level, GLint internalFormat, GLsizei width,
			  GLsizei height, GLint border, GLenum format, GLenum type,
			  const GLvoid * pixels)
{
	TRACE;
}

void
trace_glTexImage3D (GLenum target, GLint level, GLint internalFormat, GLsizei width,
			  GLsizei height, GLsizei depth, GLint border, GLenum format,
			  GLenum type, const GLvoid * pixels)
{
	TRACE;
}

void
trace_glTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
	TRACE;
}

void
trace_glTexParameterfv (GLenum target, GLenum pname, const GLfloat * params)
{
	TRACE;
}

void
trace_glTexParameteri (GLenum target, GLenum pname, GLint param)
{
	TRACE;
}

void
trace_glTexParameteriv (GLenum target, GLenum pname, const GLint * params)
{
	TRACE;
}

void
trace_glTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLsizei width,
				 GLenum format, GLenum type, const GLvoid * pixels)
{
	TRACE;
}

void
trace_glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
				 GLsizei width, GLsizei height, GLenum format, GLenum type,
				 const GLvoid * pixels)
{
	TRACE;
}

void
trace_glTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
				 GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
				 GLenum format, GLenum type, const GLvoid * pixels)
{
	TRACE;
}

void
trace_glTranslated (GLdouble x, GLdouble y, GLdouble z)
{
	TRACE;
}

void
trace_glTranslatef (GLfloat x, GLfloat y, GLfloat z)
{
	TRACE;
}

void
trace_glVertex2d (GLdouble x, GLdouble y)
{
	TRACE;
}

void
trace_glVertex2dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glVertex2f (GLfloat x, GLfloat y)
{
	TRACE;
}

void
trace_glVertex2fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glVertex2i (GLint x, GLint y)
{
	TRACE;
}

void
trace_glVertex2iv (const GLint * v)
{
	TRACE;
}

void
trace_glVertex2s (GLshort x, GLshort y)
{
	TRACE;
}

void
trace_glVertex2sv (const GLshort * v)
{
	TRACE;
}

void
trace_glVertex3d (GLdouble x, GLdouble y, GLdouble z)
{
	TRACE;
}

void
trace_glVertex3dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glVertex3f (GLfloat x, GLfloat y, GLfloat z)
{
	TRACE;
}

void
trace_glVertex3fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glVertex3i (GLint x, GLint y, GLint z)
{
	TRACE;
}

void
trace_glVertex3iv (const GLint * v)
{
	TRACE;
}

void
trace_glVertex3s (GLshort x, GLshort y, GLshort z)
{
	TRACE;
}

void
trace_glVertex3sv (const GLshort * v)
{
	TRACE;
}

void
trace_glVertex4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
	TRACE;
}

void
trace_glVertex4dv (const GLdouble * v)
{
	TRACE;
}

void
trace_glVertex4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
	TRACE;
}

void
trace_glVertex4fv (const GLfloat * v)
{
	TRACE;
}

void
trace_glVertex4i (GLint x, GLint y, GLint z, GLint w)
{
	TRACE;
}

void
trace_glVertex4iv (const GLint * v)
{
	TRACE;
}

void
trace_glVertex4s (GLshort x, GLshort y, GLshort z, GLshort w)
{
	TRACE;
}

void
trace_glVertex4sv (const GLshort * v)
{
	TRACE;
}

void
trace_glVertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr)
{
	TRACE;
}

void
trace_glViewport (GLint x, GLint y, GLsizei width, GLsizei height)
{
	TRACE;
}

void
trace_glPNTrianglesiATI (GLint x, GLint y)
{
	TRACE;
}

void
trace_glFramebufferTexture1D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
}

void
trace_glFramebufferTexture2D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
}

void
trace_glFramebufferTexture3D (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint layer)
{
}

void
trace_glFramebufferTexture (GLenum target, GLenum attachment, GLuint texture, GLint level)
{
}

void
trace_glNamedFramebufferTexture (GLuint framebuffer, GLenum attachment, GLuint texture, GLint level)
{
}

void
trace_glCreateVertexArrays (GLsizei n, GLuint *arrays)
{
}

void
trace_glBindVertexArray (GLuint array)
{
}
