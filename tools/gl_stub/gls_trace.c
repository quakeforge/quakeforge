#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>

#include "QF/GL/defines.h"
#include "QF/GL/types.h"

typedef struct __GLXcontextRec *GLXContext;
typedef XID GLXDrawable;

void
trace_glAccum (GLenum op, GLfloat value)
{
}

void
trace_glAlphaFunc (GLenum func, GLclampf ref)
{
}

GLboolean
trace_glAreTexturesResident (GLsizei n, const GLuint * textures,
					   GLboolean * residences)
{
	return 0;
}

void
trace_glArrayElement (GLint i)
{
}

void
trace_glBegin (GLenum mode)
{
}

void
trace_glBindTexture (GLenum target, GLuint texture)
{
}

void
trace_glBitmap (GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig,
		  GLfloat xmove, GLfloat ymove, const GLubyte * bitmap)
{
}

void
trace_glBlendColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
}

void
trace_glBlendEquation (GLenum mode)
{
}

void
trace_glBlendFunc (GLenum sfactor, GLenum dfactor)
{
}

void
trace_glCallList (GLuint list)
{
}

void
trace_glCallLists (GLsizei n, GLenum type, const GLvoid * lists)
{
}

void
trace_glClear (GLbitfield mask)
{
}

void
trace_glClearAccum (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
}

void
trace_glClearColor (GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
}

void
trace_glClearDepth (GLclampd depth)
{
}

void
trace_glClearIndex (GLfloat c)
{
}

void
trace_glClearStencil (GLint s)
{
}

void
trace_glClipPlane (GLenum plane, const GLdouble * equation)
{
}

void
trace_glColor3b (GLbyte red, GLbyte green, GLbyte blue)
{
}

void
trace_glColor3bv (const GLbyte * v)
{
}

void
trace_glColor3d (GLdouble red, GLdouble green, GLdouble blue)
{
}

void
trace_glColor3dv (const GLdouble * v)
{
}

void
trace_glColor3f (GLfloat red, GLfloat green, GLfloat blue)
{
}

void
trace_glColor3fv (const GLfloat * v)
{
}

void
trace_glColor3i (GLint red, GLint green, GLint blue)
{
}

void
trace_glColor3iv (const GLint * v)
{
}

void
trace_glColor3s (GLshort red, GLshort green, GLshort blue)
{
}

void
trace_glColor3sv (const GLshort * v)
{
}

void
trace_glColor3ub (GLubyte red, GLubyte green, GLubyte blue)
{
}

void
trace_glColor3ubv (const GLubyte * v)
{
}

void
trace_glColor3ui (GLuint red, GLuint green, GLuint blue)
{
}

void
trace_glColor3uiv (const GLuint * v)
{
}

void
trace_glColor3us (GLushort red, GLushort green, GLushort blue)
{
}

void
trace_glColor3usv (const GLushort * v)
{
}

void
trace_glColor4b (GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
}

void
trace_glColor4bv (const GLbyte * v)
{
}

void
trace_glColor4d (GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
}

void
trace_glColor4dv (const GLdouble * v)
{
}

void
trace_glColor4f (GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
}

void
trace_glColor4fv (const GLfloat * v)
{
}

void
trace_glColor4i (GLint red, GLint green, GLint blue, GLint alpha)
{
}

void
trace_glColor4iv (const GLint * v)
{
}

void
trace_glColor4s (GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
}

void
trace_glColor4sv (const GLshort * v)
{
}

void
trace_glColor4ub (GLubyte red, GLubyte green, GLubyte blue, GLubyte alpha)
{
}

void
trace_glColor4ubv (const GLubyte * v)
{
}

void
trace_glColor4ui (GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
}

void
trace_glColor4uiv (const GLuint * v)
{
}

void
trace_glColor4us (GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
}

void
trace_glColor4usv (const GLushort * v)
{
}

void
trace_glColorMask (GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
}

void
trace_glColorMaterial (GLenum face, GLenum mode)
{
}

void
trace_glColorPointer (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr)
{
}

void
trace_glColorSubTable (GLenum target, GLsizei start, GLsizei count, GLenum format,
				 GLenum type, const GLvoid * data)
{
}

void
trace_glColorTable (GLenum target, GLenum internalformat, GLsizei width,
			  GLenum format, GLenum type, const GLvoid * table)
{
}

void
trace_glColorTableParameterfv (GLenum target, GLenum pname, const GLfloat * params)
{
}

void
trace_glColorTableParameteriv (GLenum target, GLenum pname, const GLint * params)
{
}

void
trace_glConvolutionFilter1D (GLenum target, GLenum internalformat, GLsizei width,
					   GLenum format, GLenum type, const GLvoid * image)
{
}

void
trace_glConvolutionFilter2D (GLenum target, GLenum internalformat, GLsizei width,
					   GLsizei height, GLenum format, GLenum type,
					   const GLvoid * image)
{
}

void
trace_glConvolutionParameterf (GLenum target, GLenum pname, GLfloat params)
{
}

void
trace_glConvolutionParameterfv (GLenum target, GLenum pname, const GLfloat * params)
{
}

void
trace_glConvolutionParameteri (GLenum target, GLenum pname, GLint params)
{
}

void
trace_glConvolutionParameteriv (GLenum target, GLenum pname, const GLint * params)
{
}

void
trace_glCopyColorSubTable (GLenum target, GLsizei start, GLint x, GLint y,
					 GLsizei width)
{
}

void
trace_glCopyColorTable (GLenum target, GLenum internalformat, GLint x, GLint y,
				  GLsizei width)
{
}

void
trace_glCopyConvolutionFilter1D (GLenum target, GLenum internalformat, GLint x,
						   GLint y, GLsizei width)
{
}

void
trace_glCopyConvolutionFilter2D (GLenum target, GLenum internalformat, GLint x,
						   GLint y, GLsizei width, GLsizei height)
{
}

void
trace_glCopyPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
}

void
trace_glCopyTexImage1D (GLenum target, GLint level, GLenum internalformat, GLint x,
				  GLint y, GLsizei width, GLint border)
{
}

void
trace_glCopyTexImage2D (GLenum target, GLint level, GLenum internalformat, GLint x,
				  GLint y, GLsizei width, GLsizei height, GLint border)
{
}

void
trace_glCopyTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLint x,
					 GLint y, GLsizei width)
{
}

void
trace_glCopyTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
					 GLint x, GLint y, GLsizei width, GLsizei height)
{
}

void
trace_glCopyTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
					 GLint zoffset, GLint x, GLint y, GLsizei width,
					 GLsizei height)
{
}

void
trace_glCullFace (GLenum mode)
{
}

void
trace_glDeleteLists (GLuint list, GLsizei range)
{
}

void
trace_glDeleteTextures (GLsizei n, const GLuint * textures)
{
}

void
trace_glDepthFunc (GLenum func)
{
}

void
trace_glDepthMask (GLboolean flag)
{
}

void
trace_glDepthRange (GLclampd near_val, GLclampd far_val)
{
}

void
trace_glDisable (GLenum cap)
{
}

void
trace_glDisableClientState (GLenum cap)
{
}

void
trace_glDrawArrays (GLenum mode, GLint first, GLsizei count)
{
}

void
trace_glDrawBuffer (GLenum mode)
{
}

void
trace_glDrawElements (GLenum mode, GLsizei count, GLenum type, const GLvoid * indices)
{
}

void
trace_glDrawPixels (GLsizei width, GLsizei height, GLenum format, GLenum type,
			  const GLvoid * pixels)
{
}

void
trace_glDrawRangeElements (GLenum mode, GLuint start, GLuint end, GLsizei count,
					 GLenum type, const GLvoid * indices)
{
}

void
trace_glEdgeFlag (GLboolean flag)
{
}

void
trace_glEdgeFlagPointer (GLsizei stride, const GLvoid * ptr)
{
}

void
trace_glEdgeFlagv (const GLboolean * flag)
{
}

void
trace_glEnable (GLenum cap)
{
}

void
trace_glEnableClientState (GLenum cap)
{
}

void
trace_glEnd (void)
{
}

void
trace_glEndList (void)
{
}

void
trace_glEvalCoord1d (GLdouble u)
{
}

void
trace_glEvalCoord1dv (const GLdouble * u)
{
}

void
trace_glEvalCoord1f (GLfloat u)
{
}

void
trace_glEvalCoord1fv (const GLfloat * u)
{
}

void
trace_glEvalCoord2d (GLdouble u, GLdouble v)
{
}

void
trace_glEvalCoord2dv (const GLdouble * u)
{
}

void
trace_glEvalCoord2f (GLfloat u, GLfloat v)
{
}

void
trace_glEvalCoord2fv (const GLfloat * u)
{
}

void
trace_glEvalMesh1 (GLenum mode, GLint i1, GLint i2)
{
}

void
trace_glEvalMesh2 (GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
}

void
trace_glEvalPoint1 (GLint i)
{
}

void
trace_glEvalPoint2 (GLint i, GLint j)
{
}

void
trace_glFeedbackBuffer (GLsizei size, GLenum type, GLfloat * buffer)
{
}

void
trace_glFinish (void)
{
}

void
trace_glFlush (void)
{
}

void
trace_glFogf (GLenum pname, GLfloat param)
{
}

void
trace_glFogfv (GLenum pname, const GLfloat * params)
{
}

void
trace_glFogi (GLenum pname, GLint param)
{
}

void
trace_glFogiv (GLenum pname, const GLint * params)
{
}

void
trace_glFrontFace (GLenum mode)
{
}

void
trace_glFrustum (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
		   GLdouble near_val, GLdouble far_val)
{
}

GLuint
trace_glGenLists (GLsizei range)
{
	return 0;
}

void
trace_glGenTextures (GLsizei n, GLuint * textures)
{
}

void
trace_glGetBooleanv (GLenum pname, GLboolean * params)
{
}

void
trace_glGetClipPlane (GLenum plane, GLdouble * equation)
{
}

void
trace_glGetColorTable (GLenum target, GLenum format, GLenum type, GLvoid * table)
{
}

void
trace_glGetColorTableParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
trace_glGetColorTableParameteriv (GLenum target, GLenum pname, GLint * params)
{
}

void
trace_glGetConvolutionFilter (GLenum target, GLenum format, GLenum type,
						GLvoid * image)
{
}

void
trace_glGetConvolutionParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
trace_glGetConvolutionParameteriv (GLenum target, GLenum pname, GLint * params)
{
}

void
trace_glGetDoublev (GLenum pname, GLdouble * params)
{
}

GLenum
trace_glGetError (void)
{
	return 0;
}

void
trace_glGetFloatv (GLenum pname, GLfloat * params)
{
}

void
trace_glGetHistogram (GLenum target, GLboolean reset, GLenum format, GLenum type,
				GLvoid * values)
{
}

void
trace_glGetHistogramParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
trace_glGetHistogramParameteriv (GLenum target, GLenum pname, GLint * params)
{
}

void
trace_glGetIntegerv (GLenum pname, GLint * params)
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
trace_glGetLightfv (GLenum light, GLenum pname, GLfloat * params)
{
}

void
trace_glGetLightiv (GLenum light, GLenum pname, GLint * params)
{
}

void
trace_glGetMapdv (GLenum target, GLenum query, GLdouble * v)
{
}

void
trace_glGetMapfv (GLenum target, GLenum query, GLfloat * v)
{
}

void
trace_glGetMapiv (GLenum target, GLenum query, GLint * v)
{
}

void
trace_glGetMaterialfv (GLenum face, GLenum pname, GLfloat * params)
{
}

void
trace_glGetMaterialiv (GLenum face, GLenum pname, GLint * params)
{
}

void
trace_glGetMinmax (GLenum target, GLboolean reset, GLenum format, GLenum types,
			 GLvoid * values)
{
}

void
trace_glGetMinmaxParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
trace_glGetMinmaxParameteriv (GLenum target, GLenum pname, GLint * params)
{
}

void
trace_glGetPixelMapfv (GLenum map, GLfloat * values)
{
}

void
trace_glGetPixelMapuiv (GLenum map, GLuint * values)
{
}

void
trace_glGetPixelMapusv (GLenum map, GLushort * values)
{
}

void
trace_glGetPointerv (GLenum pname, void **params)
{
}

void
trace_glGetPolygonStipple (GLubyte * mask)
{
}

void
trace_glGetSeparableFilter (GLenum target, GLenum format, GLenum type, GLvoid * row,
					  GLvoid * column, GLvoid * span)
{
}

const GLubyte *
trace_glGetString (GLenum name)
{
	return "";
}

void
trace_glGetTexEnvfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
trace_glGetTexEnviv (GLenum target, GLenum pname, GLint * params)
{
}

void
trace_glGetTexGendv (GLenum coord, GLenum pname, GLdouble * params)
{
}

void
trace_glGetTexGenfv (GLenum coord, GLenum pname, GLfloat * params)
{
}

void
trace_glGetTexGeniv (GLenum coord, GLenum pname, GLint * params)
{
}

void
trace_glGetTexImage (GLenum target, GLint level, GLenum format, GLenum type,
			   GLvoid * pixels)
{
}

void
trace_glGetTexLevelParameterfv (GLenum target, GLint level, GLenum pname,
						  GLfloat * params)
{
}

void
trace_glGetTexLevelParameteriv (GLenum target, GLint level, GLenum pname,
						  GLint * params)
{
}

void
trace_glGetTexParameterfv (GLenum target, GLenum pname, GLfloat * params)
{
}

void
trace_glGetTexParameteriv (GLenum target, GLenum pname, GLint * params)
{
}

void
trace_glHint (GLenum target, GLenum mode)
{
}

void
trace_glHistogram (GLenum target, GLsizei width, GLenum internalformat,
			 GLboolean sink)
{
}

void
trace_glIndexMask (GLuint mask)
{
}

void
trace_glIndexPointer (GLenum type, GLsizei stride, const GLvoid * ptr)
{
}

void
trace_glIndexd (GLdouble c)
{
}

void
trace_glIndexdv (const GLdouble * c)
{
}

void
trace_glIndexf (GLfloat c)
{
}

void
trace_glIndexfv (const GLfloat * c)
{
}

void
trace_glIndexi (GLint c)
{
}

void
trace_glIndexiv (const GLint * c)
{
}

void
trace_glIndexs (GLshort c)
{
}

void
trace_glIndexsv (const GLshort * c)
{
}

void
trace_glIndexub (GLubyte c)
{
}

void
trace_glIndexubv (const GLubyte * c)
{
}

void
trace_glInitNames (void)
{
}

void
trace_glInterleavedArrays (GLenum format, GLsizei stride, const GLvoid * pointer)
{
}

GLboolean
trace_glIsEnabled (GLenum cap)
{
	return 0;
}

GLboolean
trace_glIsList (GLuint list)
{
	return 0;
}

GLboolean
trace_glIsTexture (GLuint texture)
{
	return 0;
}

void
trace_glLightModelf (GLenum pname, GLfloat param)
{
}

void
trace_glLightModelfv (GLenum pname, const GLfloat * params)
{
}

void
trace_glLightModeli (GLenum pname, GLint param)
{
}

void
trace_glLightModeliv (GLenum pname, const GLint * params)
{
}

void
trace_glLightf (GLenum light, GLenum pname, GLfloat param)
{
}

void
trace_glLightfv (GLenum light, GLenum pname, const GLfloat * params)
{
}

void
trace_glLighti (GLenum light, GLenum pname, GLint param)
{
}

void
trace_glLightiv (GLenum light, GLenum pname, const GLint * params)
{
}

void
trace_glLineStipple (GLint factor, GLushort pattern)
{
}

void
trace_glLineWidth (GLfloat width)
{
}

void
trace_glListBase (GLuint base)
{
}

void
trace_glLoadIdentity (void)
{
}

void
trace_glLoadMatrixd (const GLdouble * m)
{
}

void
trace_glLoadMatrixf (const GLfloat * m)
{
}

void
trace_glLoadName (GLuint name)
{
}

void
trace_glLogicOp (GLenum opcode)
{
}

void
trace_glMap1d (GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order,
		 const GLdouble * points)
{
}

void
trace_glMap1f (GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order,
		 const GLfloat * points)
{
}

void
trace_glMap2d (GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder,
		 GLdouble v1, GLdouble v2, GLint vstride, GLint vorder,
		 const GLdouble * points)
{
}

void
trace_glMap2f (GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder,
		 GLfloat v1, GLfloat v2, GLint vstride, GLint vorder,
		 const GLfloat * points)
{
}

void
trace_glMapGrid1d (GLint un, GLdouble u1, GLdouble u2)
{
}

void
trace_glMapGrid1f (GLint un, GLfloat u1, GLfloat u2)
{
}

void
trace_glMapGrid2d (GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1,
			 GLdouble v2)
{
}

void
trace_glMapGrid2f (GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
}

void
trace_glMaterialf (GLenum face, GLenum pname, GLfloat param)
{
}

void
trace_glMaterialfv (GLenum face, GLenum pname, const GLfloat * params)
{
}

void
trace_glMateriali (GLenum face, GLenum pname, GLint param)
{
}

void
trace_glMaterialiv (GLenum face, GLenum pname, const GLint * params)
{
}

void
trace_glMatrixMode (GLenum mode)
{
}

void
trace_glMinmax (GLenum target, GLenum internalformat, GLboolean sink)
{
}

void
trace_glMultMatrixd (const GLdouble * m)
{
}

void
trace_glMultMatrixf (const GLfloat * m)
{
}

void
trace_glNewList (GLuint list, GLenum mode)
{
}

void
trace_glNormal3b (GLbyte nx, GLbyte ny, GLbyte nz)
{
}

void
trace_glNormal3bv (const GLbyte * v)
{
}

void
trace_glNormal3d (GLdouble nx, GLdouble ny, GLdouble nz)
{
}

void
trace_glNormal3dv (const GLdouble * v)
{
}

void
trace_glNormal3f (GLfloat nx, GLfloat ny, GLfloat nz)
{
}

void
trace_glNormal3fv (const GLfloat * v)
{
}

void
trace_glNormal3i (GLint nx, GLint ny, GLint nz)
{
}

void
trace_glNormal3iv (const GLint * v)
{
}

void
trace_glNormal3s (GLshort nx, GLshort ny, GLshort nz)
{
}

void
trace_glNormal3sv (const GLshort * v)
{
}

void
trace_glNormalPointer (GLenum type, GLsizei stride, const GLvoid * ptr)
{
}

void
trace_glOrtho (GLdouble left, GLdouble right, GLdouble bottom, GLdouble top,
		 GLdouble near_val, GLdouble far_val)
{
}

void
trace_glPassThrough (GLfloat token)
{
}

void
trace_glPixelMapfv (GLenum map, GLint mapsize, const GLfloat * values)
{
}

void
trace_glPixelMapuiv (GLenum map, GLint mapsize, const GLuint * values)
{
}

void
trace_glPixelMapusv (GLenum map, GLint mapsize, const GLushort * values)
{
}

void
trace_glPixelStoref (GLenum pname, GLfloat param)
{
}

void
trace_glPixelStorei (GLenum pname, GLint param)
{
}

void
trace_glPixelTransferf (GLenum pname, GLfloat param)
{
}

void
trace_glPixelTransferi (GLenum pname, GLint param)
{
}

void
trace_glPixelZoom (GLfloat xfactor, GLfloat yfactor)
{
}

void
trace_glPointSize (GLfloat size)
{
}

void
trace_glPolygonMode (GLenum face, GLenum mode)
{
}

void
trace_glPolygonOffset (GLfloat factor, GLfloat units)
{
}

void
trace_glPolygonStipple (const GLubyte * mask)
{
}

void
trace_glPopAttrib (void)
{
}

void
trace_glPopClientAttrib (void)
{
}

void
trace_glPopMatrix (void)
{
}

void
trace_glPopName (void)
{
}

void
trace_glPrioritizeTextures (GLsizei n, const GLuint * textures,
					  const GLclampf * priorities)
{
}

void
trace_glPushAttrib (GLbitfield mask)
{
}

void
trace_glPushClientAttrib (GLbitfield mask)
{
}

void
trace_glPushMatrix (void)
{
}

void
trace_glPushName (GLuint name)
{
}

void
trace_glRasterPos2d (GLdouble x, GLdouble y)
{
}

void
trace_glRasterPos2dv (const GLdouble * v)
{
}

void
trace_glRasterPos2f (GLfloat x, GLfloat y)
{
}

void
trace_glRasterPos2fv (const GLfloat * v)
{
}

void
trace_glRasterPos2i (GLint x, GLint y)
{
}

void
trace_glRasterPos2iv (const GLint * v)
{
}

void
trace_glRasterPos2s (GLshort x, GLshort y)
{
}

void
trace_glRasterPos2sv (const GLshort * v)
{
}

void
trace_glRasterPos3d (GLdouble x, GLdouble y, GLdouble z)
{
}

void
trace_glRasterPos3dv (const GLdouble * v)
{
}

void
trace_glRasterPos3f (GLfloat x, GLfloat y, GLfloat z)
{
}

void
trace_glRasterPos3fv (const GLfloat * v)
{
}

void
trace_glRasterPos3i (GLint x, GLint y, GLint z)
{
}

void
trace_glRasterPos3iv (const GLint * v)
{
}

void
trace_glRasterPos3s (GLshort x, GLshort y, GLshort z)
{
}

void
trace_glRasterPos3sv (const GLshort * v)
{
}

void
trace_glRasterPos4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
}

void
trace_glRasterPos4dv (const GLdouble * v)
{
}

void
trace_glRasterPos4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
}

void
trace_glRasterPos4fv (const GLfloat * v)
{
}

void
trace_glRasterPos4i (GLint x, GLint y, GLint z, GLint w)
{
}

void
trace_glRasterPos4iv (const GLint * v)
{
}

void
trace_glRasterPos4s (GLshort x, GLshort y, GLshort z, GLshort w)
{
}

void
trace_glRasterPos4sv (const GLshort * v)
{
}

void
trace_glReadBuffer (GLenum mode)
{
}

void
trace_glReadPixels (GLint x, GLint y, GLsizei width, GLsizei height, GLenum format,
			  GLenum type, GLvoid * pixels)
{
}

void
trace_glRectd (GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
}

void
trace_glRectdv (const GLdouble * v1, const GLdouble * v2)
{
}

void
trace_glRectf (GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
}

void
trace_glRectfv (const GLfloat * v1, const GLfloat * v2)
{
}

void
trace_glRecti (GLint x1, GLint y1, GLint x2, GLint y2)
{
}

void
trace_glRectiv (const GLint * v1, const GLint * v2)
{
}

void
trace_glRects (GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
}

void
trace_glRectsv (const GLshort * v1, const GLshort * v2)
{
}

GLint
trace_glRenderMode (GLenum mode)
{
	return 0;
}

void
trace_glResetHistogram (GLenum target)
{
}

void
trace_glResetMinmax (GLenum target)
{
}

void
trace_glRotated (GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
}

void
trace_glRotatef (GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
}

void
trace_glScaled (GLdouble x, GLdouble y, GLdouble z)
{
}

void
trace_glScalef (GLfloat x, GLfloat y, GLfloat z)
{
}

void
trace_glScissor (GLint x, GLint y, GLsizei width, GLsizei height)
{
}

void
trace_glSelectBuffer (GLsizei size, GLuint * buffer)
{
}

void
trace_glSeparableFilter2D (GLenum target, GLenum internalformat, GLsizei width,
					 GLsizei height, GLenum format, GLenum type,
					 const GLvoid * row, const GLvoid * column)
{
}

void
trace_glShadeModel (GLenum mode)
{
}

void
trace_glStencilFunc (GLenum func, GLint ref, GLuint mask)
{
}

void
trace_glStencilMask (GLuint mask)
{
}

void
trace_glStencilOp (GLenum fail, GLenum zfail, GLenum zpass)
{
}

void
trace_glTexCoord1d (GLdouble s)
{
}

void
trace_glTexCoord1dv (const GLdouble * v)
{
}

void
trace_glTexCoord1f (GLfloat s)
{
}

void
trace_glTexCoord1fv (const GLfloat * v)
{
}

void
trace_glTexCoord1i (GLint s)
{
}

void
trace_glTexCoord1iv (const GLint * v)
{
}

void
trace_glTexCoord1s (GLshort s)
{
}

void
trace_glTexCoord1sv (const GLshort * v)
{
}

void
trace_glTexCoord2d (GLdouble s, GLdouble t)
{
}

void
trace_glTexCoord2dv (const GLdouble * v)
{
}

void
trace_glTexCoord2f (GLfloat s, GLfloat t)
{
}

void
trace_glTexCoord2fv (const GLfloat * v)
{
}

void
trace_glTexCoord2i (GLint s, GLint t)
{
}

void
trace_glTexCoord2iv (const GLint * v)
{
}

void
trace_glTexCoord2s (GLshort s, GLshort t)
{
}

void
trace_glTexCoord2sv (const GLshort * v)
{
}

void
trace_glTexCoord3d (GLdouble s, GLdouble t, GLdouble r)
{
}

void
trace_glTexCoord3dv (const GLdouble * v)
{
}

void
trace_glTexCoord3f (GLfloat s, GLfloat t, GLfloat r)
{
}

void
trace_glTexCoord3fv (const GLfloat * v)
{
}

void
trace_glTexCoord3i (GLint s, GLint t, GLint r)
{
}

void
trace_glTexCoord3iv (const GLint * v)
{
}

void
trace_glTexCoord3s (GLshort s, GLshort t, GLshort r)
{
}

void
trace_glTexCoord3sv (const GLshort * v)
{
}

void
trace_glTexCoord4d (GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
}

void
trace_glTexCoord4dv (const GLdouble * v)
{
}

void
trace_glTexCoord4f (GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
}

void
trace_glTexCoord4fv (const GLfloat * v)
{
}

void
trace_glTexCoord4i (GLint s, GLint t, GLint r, GLint q)
{
}

void
trace_glTexCoord4iv (const GLint * v)
{
}

void
trace_glTexCoord4s (GLshort s, GLshort t, GLshort r, GLshort q)
{
}

void
trace_glTexCoord4sv (const GLshort * v)
{
}

void
trace_glTexCoordPointer (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr)
{
}

void
trace_glTexEnvf (GLenum target, GLenum pname, GLfloat param)
{
}

void
trace_glTexEnvfv (GLenum target, GLenum pname, const GLfloat * params)
{
}

void
trace_glTexEnvi (GLenum target, GLenum pname, GLint param)
{
}

void
trace_glTexEnviv (GLenum target, GLenum pname, const GLint * params)
{
}

void
trace_glTexGend (GLenum coord, GLenum pname, GLdouble param)
{
}

void
trace_glTexGendv (GLenum coord, GLenum pname, const GLdouble * params)
{
}

void
trace_glTexGenf (GLenum coord, GLenum pname, GLfloat param)
{
}

void
trace_glTexGenfv (GLenum coord, GLenum pname, const GLfloat * params)
{
}

void
trace_glTexGeni (GLenum coord, GLenum pname, GLint param)
{
}

void
trace_glTexGeniv (GLenum coord, GLenum pname, const GLint * params)
{
}

void
trace_glTexImage1D (GLenum target, GLint level, GLint internalFormat, GLsizei width,
			  GLint border, GLenum format, GLenum type, const GLvoid * pixels)
{
}

void
trace_glTexImage2D (GLenum target, GLint level, GLint internalFormat, GLsizei width,
			  GLsizei height, GLint border, GLenum format, GLenum type,
			  const GLvoid * pixels)
{
}

void
trace_glTexImage3D (GLenum target, GLint level, GLint internalFormat, GLsizei width,
			  GLsizei height, GLsizei depth, GLint border, GLenum format,
			  GLenum type, const GLvoid * pixels)
{
}

void
trace_glTexParameterf (GLenum target, GLenum pname, GLfloat param)
{
}

void
trace_glTexParameterfv (GLenum target, GLenum pname, const GLfloat * params)
{
}

void
trace_glTexParameteri (GLenum target, GLenum pname, GLint param)
{
}

void
trace_glTexParameteriv (GLenum target, GLenum pname, const GLint * params)
{
}

void
trace_glTexSubImage1D (GLenum target, GLint level, GLint xoffset, GLsizei width,
				 GLenum format, GLenum type, const GLvoid * pixels)
{
}

void
trace_glTexSubImage2D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
				 GLsizei width, GLsizei height, GLenum format, GLenum type,
				 const GLvoid * pixels)
{
}

void
trace_glTexSubImage3D (GLenum target, GLint level, GLint xoffset, GLint yoffset,
				 GLint zoffset, GLsizei width, GLsizei height, GLsizei depth,
				 GLenum format, GLenum type, const GLvoid * pixels)
{
}

void
trace_glTranslated (GLdouble x, GLdouble y, GLdouble z)
{
}

void
trace_glTranslatef (GLfloat x, GLfloat y, GLfloat z)
{
}

void
trace_glVertex2d (GLdouble x, GLdouble y)
{
}

void
trace_glVertex2dv (const GLdouble * v)
{
}

void
trace_glVertex2f (GLfloat x, GLfloat y)
{
}

void
trace_glVertex2fv (const GLfloat * v)
{
}

void
trace_glVertex2i (GLint x, GLint y)
{
}

void
trace_glVertex2iv (const GLint * v)
{
}

void
trace_glVertex2s (GLshort x, GLshort y)
{
}

void
trace_glVertex2sv (const GLshort * v)
{
}

void
trace_glVertex3d (GLdouble x, GLdouble y, GLdouble z)
{
}

void
trace_glVertex3dv (const GLdouble * v)
{
}

void
trace_glVertex3f (GLfloat x, GLfloat y, GLfloat z)
{
}

void
trace_glVertex3fv (const GLfloat * v)
{
}

void
trace_glVertex3i (GLint x, GLint y, GLint z)
{
}

void
trace_glVertex3iv (const GLint * v)
{
}

void
trace_glVertex3s (GLshort x, GLshort y, GLshort z)
{
}

void
trace_glVertex3sv (const GLshort * v)
{
}

void
trace_glVertex4d (GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
}

void
trace_glVertex4dv (const GLdouble * v)
{
}

void
trace_glVertex4f (GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
}

void
trace_glVertex4fv (const GLfloat * v)
{
}

void
trace_glVertex4i (GLint x, GLint y, GLint z, GLint w)
{
}

void
trace_glVertex4iv (const GLint * v)
{
}

void
trace_glVertex4s (GLshort x, GLshort y, GLshort z, GLshort w)
{
}

void
trace_glVertex4sv (const GLshort * v)
{
}

void
trace_glVertexPointer (GLint size, GLenum type, GLsizei stride, const GLvoid * ptr)
{
}

void
trace_glViewport (GLint x, GLint y, GLsizei width, GLsizei height)
{
}

void
trace_glXSwapBuffers (Display *dpy, GLXDrawable drawable)
{
}

XVisualInfo*
trace_glXChooseVisual (Display *dpy, int screen, int *attribList)
{
	XVisualInfo template;
	int         num_visuals;
	int         template_mask;

	template_mask = 0;
	template.visualid =
		XVisualIDFromVisual (XDefaultVisual (dpy, screen));
	template_mask = VisualIDMask;
	return XGetVisualInfo (dpy, template_mask, &template, &num_visuals);
}

GLXContext
trace_glXCreateContext (Display *dpy, XVisualInfo *vis, GLXContext shareList,
				  Bool direct)
{
	return (GLXContext)1;
}

Bool
trace_glXMakeCurrent (Display *dpy, GLXDrawable drawable, GLXContext ctx)
{
	return 1;
}

void
trace_glXDestroyContext (Display *dpy, GLXContext ctx )
{
}

int
trace_glXGetConfig (Display *dpy, XVisualInfo *visual, int attrib, int *value )
{
    return 0;
}
