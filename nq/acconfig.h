/*
	Compiler/Machine-Specific Configuration
*/
#ifndef _CONFIG_H_
#define _CONFIG_H_
@TOP@
/* "Proper" package name */
#undef PROGRAM

/* Define this to the Quake version you support */
#undef QUAKE_VERSION

/* Define this to the QSG standard version you support */
#undef QSG_VERSION

/* Define if you want to use QF-style defaults instead of Id-style */
#undef NEWSTYLE

/* Define this to the location of the global config file */
#undef FS_GLOBALCFG

/* Define this to the shared game directory root */
#undef FS_SHAREPATH

/* Define this to the unshared game directory root */
#undef FS_USERPATH

/* Define this to the base game for the engine to load */
#undef BASEGAME

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

/* Define this if you have 3dfx */
#undef HAVE_TDFXGL

/* Define this if you have GL_COLOR_INDEX8_EXT in GL/gl.h */
#undef HAVE_GL_COLOR_INDEX8_EXT

/* Define this if you are using a version of Mesa with X mode change support */
#undef HAVE_XMESA

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

/* If your version of OpenGL uses APIENTRY, define GLAPIENTRY to be APIENTRY */
#undef GLAPIENTRY

/* Define this to something sane if you don't have stricmp */
#undef stricmp

/* Define this if FPOS_T is a struct */
#undef HAVE_FPOS_T_STRUCT

@BOTTOM@
#endif // _CONFIG_H_
