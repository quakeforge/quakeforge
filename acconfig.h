/*
	Compiler/Machine-Specific Configuration
*/
#ifndef __config_h_
#define __config_h_
@TOP@
/* "Proper" package name */
#undef PROGRAM

/* Define this to the NetQuake standard version you support */
#undef NQ_VERSION

/* Define this to the QSG standard version you support in NetQuake */
#undef NQ_QSG_VERSION

/* Define this to the QuakeWorld standard version you support */
#undef QW_VERSION

/* Define this to the QSG standard version you support in QuakeWorld */
#undef QW_QSG_VERSION

/* Define this to the location of the global config file */
#undef FS_GLOBALCFG

/* Define this to the location of the user config file */
#undef FS_USERCFG

/* Define this to the path from which to load plugins */
#undef FS_PLUGINPATH

/* Define this to the shared game directory root */
#undef FS_SHAREPATH

/* Define this to the unshared game directory root */
#undef FS_USERPATH

/* Define this to use experimental code */
#undef _EXPERIMENTAL_

/* Define this if you want to use Intel assembly optimizations */
#undef USE_INTEL_ASM

/* Define this if you have a Linux-style CD-ROM API */
#undef USE_LINUX_CD

/* Define this if you have a BSD-style CD-ROM API */
#undef USE_BSD_CD

/* Define if you have the XFree86 DGA extension */
#undef HAVE_DGA

/* Define if you have the XFree86 VIDMODE extension */
#undef HAVE_VIDMODE

/* Define this if you have GLX */
#undef HAVE_GLX

/* Define this if you have Glide */
#undef HAVE_GLIDE

/* Define this if you have GL_COLOR_INDEX8_EXT in GL/gl.h */
#undef HAVE_GL_COLOR_INDEX8_EXT

/* Define this if you want IPv6 support */
#undef HAVE_IPV6

/* Define this if C symbols are prefixed with an underscore */
#undef HAVE_SYM_PREFIX_UNDERSCORE

/* Define this if your system has socklen_t */
#undef HAVE_SOCKLEN_T

/* Define this if your system has size_t */
#undef HAVE_SIZE_T

/* Define this if you have ss_len member in struct sockaddr_storage (BSD) */
#undef HAVE_SS_LEN

/* Define this if you have sin6_len member in struct sockaddr_in6 (BSD) */
#undef HAVE_SIN6_LEN

/* Define this if you have sa_len member in struct sockaddr (BSD) */
#undef HAVE_SA_LEN

/* Define if you have the dlopen function.  */
#undef HAVE_DLOPEN

/* Define if you have zlib */
#undef HAVE_ZLIB

/* Define if you have pthread support. */
#undef HAVE_LIBPTHREAD

/* If your version of OpenGL uses APIENTRY, define GLAPIENTRY to be APIENTRY */
#undef GLAPIENTRY

/* Define this if you have fnmatch.h */
#undef HAVE_FNMATCH_H

/* Define this if fnmatch is prototyped in fnmatch.h */
#undef HAVE_FNMATCH_PROTO

/* Define this if strnlen is prototyped in string.h */
#undef HAVE_STRNLEN_PROTO

/* Define this if fnmatch is prototyped in string.h */
#undef HAVE_STRCASESTR_PROTO

/* Define this to something appropriate for declaring 0 length arrays */
#undef ZERO_LENGTH_ARRAY

/* Define this if you have FB_AUX_VGA_PLANES_VGA4 */
#undef HAVE_FB_AUX_VGA_PLANES_VGA4

/* Define this if you have FB_AUX_VGA_PLANES_VGA4 */
#undef HAVE_FB_AUX_VGA_PLANES_CFB4

/* Define this if you have FB_AUX_VGA_PLANES_VGA4 */
#undef HAVE_FB_AUX_VGA_PLANES_CFB8

/* Define this if FPOS_T is a struct */
#undef HAVE_FPOS_T_STRUCT

/* Define strcasecmp as stricmp if you have one but not the other */
#undef strcasecmp

/* Define this if you have access */
#undef HAVE_access

/* Define this if you have _access */
#undef HAVE__access

/* Define this if you have mkdir */
#undef HAVE_mkdir

/* Define this if you have _mkdir */
#undef HAVE__mkdir

/* Define this if you want progs typechecking */
#undef TYPECHECK_PROGS

/* list of server plugins and prototypes */
#undef SERVER_PLUGIN_LIST
#undef SERVER_PLUGIN_PROTOS

/* list of client plugins and prototypes */
#undef CLIENT_PLUGIN_LIST
#undef CLIENT_PLUGIN_PROTOS

/* list of cd plugins and prototypes */
#undef CD_PLUGIN_LIST
#undef CD_PLUGIN_PROTOS

/* list of sound output plugins and prototypes */
#undef SND_OUTPUT_LIST
#undef SND_OUTPUT_PROTOS

/* list of sound render plugins and prototypes */
#undef SND_RENDER_LIST
#undef SND_RENDER_PROTOS

/* Define this to your operating system's path separator character */
#undef PATH_SEPARATOR

/* Define this if you want to use the C preprocessor */
#undef USE_CPP

/* Define this to the command line for the C preprocessor */
#undef CPP_NAME

#if defined (WIN32) && !defined(_WIN32)
# define _WIN32
#endif
#if defined (_WIN32) && !defined(WIN32)
# define WIN32
#endif

@BOTTOM@
#endif // __config_h_
