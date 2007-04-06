/* include/config.h.  Generated from config.h.in by configure.  */
/* include/config.h.in.  Generated from configure.ac by autoheader.  */

/* list of cd plugins */
#define CD_PLUGIN_LIST {"cd_win", cd_win_PluginInfo},{0, 0}

/* list of cd prototypes */
#define CD_PLUGIN_PROTOS extern plugin_t *cd_win_PluginInfo (void);

/* list of client plugins */
#define CLIENT_PLUGIN_LIST {"console_client", console_client_PluginInfo},{0, 0}

/* list of client prototypes */
#define CLIENT_PLUGIN_PROTOS extern plugin_t *console_client_PluginInfo (void);

/* Define this to the command line for the C preprocessor */
#define CPP_NAME "cpp %d -o %o %i"

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
/* #undef CRAY_STACKSEG_END */

/* Define to 1 if using `alloca.c'. */
/* #undef C_ALLOCA */

/* Define this to the location of the global config file */
#define FS_GLOBALCFG "~/quakeforge.conf"

/* Define this to the path from which to load plugins */
#define FS_PLUGINPATH "/usr/local/lib/quakeforge"

/* Define this to the shared game directory root */
#define FS_SHAREPATH "."

/* Define this to the location of the user config file */
#define FS_USERCFG "~/quakeforgerc"

/* Define this to the unshared game directory root */
#define FS_USERPATH "."

/* Define this to the default GL dynamic lib */
#define GL_DRIVER "OPENGL32.DLL"

/* Define to 1 if you have the `access' function. */
#define HAVE_ACCESS 1

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
/* #undef HAVE_ALLOCA_H */

/* Define this if alloca is prototyped */
/* #undef HAVE_ALLOCA_PROTO */
#ifndef HAVE_ALLOCA_PROTO
#ifndef QFASM
void *alloca (int size);
#endif
#endif

/* Define to 1 if you have the <alsa/asoundlib.h> header file. */
/* #undef HAVE_ALSA_ASOUNDLIB_H */

/* Define to 1 if you have the <arpa/inet.h> header file. */
/* #undef HAVE_ARPA_INET_H */

/* Define to 1 if you have the <asm/io.h> header file. */
/* #undef HAVE_ASM_IO_H */

/* Define to 1 if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 1

/* Define to 1 if you have the <conio.h> header file. */
#define HAVE_CONIO_H 1

/* Define to 1 if you have the `connect' function. */
#define HAVE_CONNECT 1

/* Define to 1 if you have the <ctype.h> header file. */
#define HAVE_CTYPE_H 1

/* Define to 1 if you have the <curses.h> header file. */
/* #undef HAVE_CURSES_H */

/* Define to 1 if you have the <ddraw.h> header file. */
#define HAVE_DDRAW_H 1

/* Define if you have the XFree86 DGA extension */
/* #undef HAVE_DGA */

/* Define to 1 if you have the <dinput.h> header file. */
#define HAVE_DINPUT_H 1

/* Define to 1 if you have the <direct.h> header file. */
#define HAVE_DIRECT_H 1

/* Define to 1 if you have the <dirent.h> header file. */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Define if you have the dlopen function. */
/* #undef HAVE_DLOPEN */

/* Define to 1 if you have the <dmedia/audio.h> header file. */
/* #undef HAVE_DMEDIA_AUDIO_H */

/* Define to 1 if you have the <dmedia/cdaudio.h> header file. */
/* #undef HAVE_DMEDIA_CDAUDIO_H */

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define to 1 if you have the <dpmi.h> header file. */
/* #undef HAVE_DPMI_H */

/* Define to 1 if you have the <dsound.h> header file. */
#define HAVE_DSOUND_H 1

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the <execinfo.h> header file. */
/* #undef HAVE_EXECINFO_H */

/* Define this if you have FB_AUX_VGA_PLANES_CFB4 */
/* #undef HAVE_FB_AUX_VGA_PLANES_CFB4 */

/* Define this if you have FB_AUX_VGA_PLANES_CFB4 */
/* #undef HAVE_FB_AUX_VGA_PLANES_CFB8 */

/* Define this if you have FB_AUX_VGA_PLANES_VGA4 */
/* #undef HAVE_FB_AUX_VGA_PLANES_VGA4 */

/* Define to 1 if you have the `fcntl' function. */
/* #undef HAVE_FCNTL */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* define this if you have flac libs */
/* #undef HAVE_FLAC */

/* Define to 1 if you have the <fnmatch.h> header file. */
#define HAVE_FNMATCH_H 1

/* Define this if fnmatch is prototyped in fnmatch.h */
/* #undef HAVE_FNMATCH_PROTO */

/* Define this if FPOS_T is a struct */
/* #undef HAVE_FPOS_T_STRUCT */

/* Define to 1 if you have the `ftime' function. */
#define HAVE_FTIME 1

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `gethostbyname' function. */
#define HAVE_GETHOSTBYNAME 1

/* Define to 1 if you have the `gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have the `getnameinfo' function. */
#define HAVE_GETNAMEINFO 1

/* Define to 1 if you have the `getpagesize' function. */
/* #undef HAVE_GETPAGESIZE */

/* Define to 1 if you have the `gettimeofday' function. */
/* #undef HAVE_GETTIMEOFDAY */

/* Define to 1 if you have the `getwd' function. */
/* #undef HAVE_GETWD */

/* Define to 1 if you have the <inttypes.h> header file. */
/* #undef HAVE_INTTYPES_H */

/* Define this if your system has struct in_pktinfo */
/* #undef HAVE_IN_PKTINFO */

/* Define to 1 if you have the <io.h> header file. */
#define HAVE_IO_H 1

/* Define this if you want IPv6 support */
/* #undef HAVE_IPV6 */

/* Define if you have libjack */
/* #undef HAVE_JACK */

/* Define to 1 if you have a functional curl library. */
#define HAVE_LIBCURL 1

/* Define to 1 if you have the <libc.h> header file. */
/* #undef HAVE_LIBC_H */

/* Define to 1 if you have the `db' library (-ldb). */
/* #undef HAVE_LIBDB */

/* Define to 1 if you have the `efence' library (-lefence). */
/* #undef HAVE_LIBEFENCE */

/* Define to 1 if you have the `m' library (-lm). */
/* #undef HAVE_LIBM */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <linux/cdrom.h> header file. */
/* #undef HAVE_LINUX_CDROM_H */

/* Define to 1 if you have the <linux/fb.h> header file. */
/* #undef HAVE_LINUX_FB_H */

/* Define to 1 if you have the <linux/joystick.h> header file. */
/* #undef HAVE_LINUX_JOYSTICK_H */

/* Define to 1 if you have the <linux/soundcard.h> header file. */
/* #undef HAVE_LINUX_SOUNDCARD_H */

/* Define to 1 if you support file names longer than 14 characters. */
#define HAVE_LONG_FILE_NAMES 1

/* Define to 1 if you have the <machine/soundcard.h> header file. */
/* #undef HAVE_MACHINE_SOUNDCARD_H */

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* Define to 1 if you have the <math.h> header file. */
#define HAVE_MATH_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <mgraph.h> header file. */
/* #undef HAVE_MGRAPH_H */

/* Define to 1 if you have the `mkdir' function. */
#define HAVE_MKDIR 1

/* Define to 1 if you have a working `mmap' system call. */
/* #undef HAVE_MMAP */

/* Define to 1 if you have the `mprotect' function. */
/* #undef HAVE_MPROTECT */

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <netdb.h> header file. */
/* #undef HAVE_NETDB_H */

/* Define to 1 if you have the <netinet/in.h> header file. */
/* #undef HAVE_NETINET_IN_H */

/* Define if you have libpng */
/* #undef HAVE_PNG */

/* Define to 1 if you have the <process.h> header file. */
#define HAVE_PROCESS_H 1

/* Define to 1 if you have the <pthread.h> header file. */
/* #undef HAVE_PTHREAD_H */

/* Define to 1 if you have the `putenv' function. */
#define HAVE_PUTENV 1

/* Define to 1 if you have the <pwd.h> header file. */
/* #undef HAVE_PWD_H */

/* Define to 1 if you have the <rpc/types.h> header file. */
/* #undef HAVE_RPC_TYPES_H */

/* Define this if you have sa_len member in struct sockaddr (BSD) */
/* #undef HAVE_SA_LEN */

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the <setjmp.h> header file. */
#define HAVE_SETJMP_H 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define this if you have sin6_len member in struct sockaddr_in6 (BSD) */
/* #undef HAVE_SIN6_LEN */

/* Define this if your system has size_t */
#define HAVE_SIZE_T 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the `socket' function. */
#define HAVE_SOCKET 1

/* Define this if your system has socklen_t */
/* #undef HAVE_SOCKLEN_T */

/* Define this if you have ss_len member in struct sockaddr_storage (BSD) */
/* #undef HAVE_SS_LEN */

/* Define to 1 if you have the `stat' function. */
#define HAVE_STAT 1

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if you have the <stdint.h> header file. */
/* #undef HAVE_STDINT_H 1 */

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasestr' function. */
/* #undef HAVE_STRCASESTR */

/* Define this if strcasestr is prototyped in string.h */
/* #undef HAVE_STRCASESTR_PROTO */

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the <strings.h> header file. */
/* #undef HAVE_STRINGS_H 1 */

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strnlen' function. */
#define HAVE_STRNLEN 1

/* Define this if strnlen is prototyped in string.h */
#define HAVE_STRNLEN_PROTO 1

/* Define to 1 if you have the `strsep' function. */
/* #undef HAVE_STRSEP */

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if `st_blksize' is member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_BLKSIZE */

/* Define to 1 if your `struct stat' has `st_blksize'. Deprecated, use
   `HAVE_STRUCT_STAT_ST_BLKSIZE' instead. */
/* #undef HAVE_ST_BLKSIZE */

/* Define this if C symbols are prefixed with an underscore */
#define HAVE_SYM_PREFIX_UNDERSCORE 1

/* Define to 1 if you have the <sys/asoundlib.h> header file. */
/* #undef HAVE_SYS_ASOUNDLIB_H */

/* Define to 1 if you have the <sys/audioio.h> header file. */
/* #undef HAVE_SYS_AUDIOIO_H */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/filio.h> header file. */
/* #undef HAVE_SYS_FILIO_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
/* #undef HAVE_SYS_IOCTL_H */

/* Define to 1 if you have the <sys/io.h> header file. */
/* #undef HAVE_SYS_IO_H */

/* Define to 1 if you have the <sys/ipc.h> header file. */
/* #undef HAVE_SYS_IPC_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
/* #undef HAVE_SYS_MMAN_H */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
/* #undef HAVE_SYS_PARAM_H */

/* Define to 1 if you have the <sys/poll.h> header file. */
/* #undef HAVE_SYS_POLL_H */

/* Define to 1 if you have the <sys/shm.h> header file. */
/* #undef HAVE_SYS_SHM_H */

/* Define to 1 if you have the <sys/signal.h> header file. */
/* #undef HAVE_SYS_SIGNAL_H */

/* Define to 1 if you have the <sys/socket.h> header file. */
/* #undef HAVE_SYS_SOCKET_H */

/* Define to 1 if you have the <sys/soundcard.h> header file. */
/* #undef HAVE_SYS_SOUNDCARD_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
/* #undef HAVE_SYS_TIME_H */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/uio.h> header file. */
/* #undef HAVE_SYS_UIO_H */

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
/* #undef HAVE_SYS_WAIT_H */

/* Define this if you have tchar.h */
#define HAVE_TCHAR_H 1

/* Define to 1 if you have the <termios.h> header file. */
/* #undef HAVE_TERMIOS_H */

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define to 1 if you have the <unistd.h> header file. */
/* #undef HAVE_UNISTD_H */

/* Define to 1 if you have the `usleep' function. */
/* #undef HAVE_USLEEP */

/* Define if va_copy is available */
/* #undef HAVE_VA_COPY */

/* Define to 1 if you have the <vgakeyboard.h> header file. */
/* #undef HAVE_VGAKEYBOARD_H */

/* Define to 1 if you have the <vgamouse.h> header file. */
/* #undef HAVE_VGAMOUSE_H */

/* Define if you have the XFree86 VIDMODE extension */
/* #undef HAVE_VIDMODE */

/* define this if you have ogg/vorbis libs */
/* #undef HAVE_VORBIS */

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the `vsnprintf' function. */
#define HAVE_VSNPRINTF 1

/* Define if you have WildMidi */
/* #undef HAVE_WILDMIDI */

/* Define to 1 if you have the <windows.h> header file. */
#define HAVE_WINDOWS_H 1

/* Define to 1 if you have the <winsock.h> header file. */
#define HAVE_WINSOCK_H 1

/* Define if you have XMMS */
/* #undef HAVE_XMMS */

/* Define if you have zlib */
#define HAVE_ZLIB 1

/* Define to 1 if you have the `_access' function. */
#define HAVE__ACCESS 1

/* Define to 1 if you have the `_ftime' function. */
#define HAVE__FTIME 1

/* Define to 1 if you have the <_mingw.h> header file. */
/* #undef HAVE__MINGW_H */

/* Define to 1 if you have the `_mkdir' function. */
#define HAVE__MKDIR 1

/* Define this if you have _SC_PAGESIZE */
/* #undef HAVE__SC_PAGESIZE */

/* Define to 1 if you have the `_snprintf' function. */
#define HAVE__SNPRINTF 1

/* Define if __va_copy is available */
/* #undef HAVE__VA_COPY */

/* Define to 1 if you have the `_vsnprintf' function. */
#define HAVE__VSNPRINTF 1

/* Define this if the GCC __attribute__ keyword is available */
/* #undef HAVE___ATTRIBUTE__ */

#ifndef HAVE___ATTRIBUTE__
# define __attribute__(x)
#endif

/* Define this if the GCC __attribute__ keyword is available */
/* #undef HAVE___ATTRIBUTE__VISIBILITY */

#ifdef HAVE___ATTRIBUTE__VISIBILITY
# define VISIBLE __attribute__((visibility ("default")))
#else
# define VISIBLE
#endif

/* Define this if the GCC __builtin_expect keyword is available */
/* #undef HAVE___BUILTIN_EXPECT */

#ifndef HAVE___BUILTIN_EXPECT
# define __builtin_expect(x,c) x
#endif

/* Defined if libcurl supports AsynchDNS */
/* #undef LIBCURL_FEATURE_ASYNCHDNS */

/* Defined if libcurl supports IDN */
#define LIBCURL_FEATURE_IDN 1

/* Defined if libcurl supports IPv6 */
#define LIBCURL_FEATURE_IPV6 1

/* Defined if libcurl supports KRB4 */
/* #undef LIBCURL_FEATURE_KRB4 */

/* Defined if libcurl supports libz */
#define LIBCURL_FEATURE_LIBZ 1

/* Defined if libcurl supports NTLM */
#define LIBCURL_FEATURE_NTLM 1

/* Defined if libcurl supports SSL */
/* #undef LIBCURL_FEATURE_SSL */

/* Defined if libcurl supports SSPI */
#define LIBCURL_FEATURE_SSPI 1

/* Defined if libcurl supports DICT */
/* #undef LIBCURL_PROTOCOL_DICT */

/* Defined if libcurl supports FILE */
/* #undef LIBCURL_PROTOCOL_FILE */

/* Defined if libcurl supports FTP */
#define LIBCURL_PROTOCOL_FTP 1

/* Defined if libcurl supports FTPS */
/* #undef LIBCURL_PROTOCOL_FTPS */

/* Defined if libcurl supports HTTP */
#define LIBCURL_PROTOCOL_HTTP 1

/* Defined if libcurl supports HTTPS */
/* #undef LIBCURL_PROTOCOL_HTTPS */

/* Defined if libcurl supports LDAP */
/* #undef LIBCURL_PROTOCOL_LDAP */

/* Defined if libcurl supports TELNET */
/* #undef LIBCURL_PROTOCOL_TELNET */

/* Defined if libcurl supports TFTP */
/* #undef LIBCURL_PROTOCOL_TFTP */

/* Define to 1 if `major', `minor', and `makedev' are declared in <mkdev.h>.
   */
/* #undef MAJOR_IN_MKDEV */

/* Define to 1 if `major', `minor', and `makedev' are declared in
   <sysmacros.h>. */
/* #undef MAJOR_IN_SYSMACROS */

/* Define this to the QSG standard version you support in NetQuake */
#define NQ_QSG_VERSION "1.0"

/* Define this to the NetQuake standard version you support */
#define NQ_VERSION "1.09"

/* Name of package */
#define PACKAGE "quakeforge"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Define this to your operating system's path separator character */
#define PATH_SEPARATOR '/'

/* "Proper" package name */
#define PROGRAM "QuakeForge"

/* Define this to where qfcc should look for header files */
#define QFCC_INCLUDE_PATH "/usr/local/include/QF/ruamoko"

/* Define this to where qfcc should look for lib files */
#define QFCC_LIB_PATH "/usr/local/lib/ruamoko"

/* Define this to the QSG standard version you support in QuakeWorld */
#define QW_QSG_VERSION "2.0"

/* Define this to the QuakeWorld standard version you support */
#define QW_VERSION "2.40"

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* list of server plugins */
#define SERVER_PLUGIN_LIST {"console_server", console_server_PluginInfo},{0, 0}

/* list of server prototypes */
#define SERVER_PLUGIN_PROTOS extern plugin_t *console_server_PluginInfo (void);

/* Define this to the default sound output driver. */
#define SND_OUTPUT_DEFAULT "win"

/* list of sound output plugins */
#define SND_OUTPUT_LIST {"snd_output_win", snd_output_win_PluginInfo},{0, 0}

/* list of sound output prototypes */
#define SND_OUTPUT_PROTOS extern plugin_t *snd_output_win_PluginInfo (void);

/* list of sound render plugins */
#define SND_RENDER_LIST {"snd_render_default", snd_render_default_PluginInfo},{0, 0}

/* list of sound render prototypes */
#define SND_RENDER_PROTOS extern plugin_t *snd_render_default_PluginInfo (void);

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define this if you are building static plugins */
#define STATIC_PLUGINS 1

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Define this if you want progs typechecking */
/* #undef TYPECHECK_PROGS */

/* Define this if you want to use Intel assembly optimizations */
/* #undef USE_INTEL_ASM */

/* Define if va_list is an array */
/* #undef VA_LIST_IS_ARRAY */

/* Version number of package */
#define VERSION "0.5.5-SVN"

/* Define to 1 if your processor stores words with the most significant byte
   first (like Motorola and SPARC, unlike Intel and VAX). */
/* #undef WORDS_BIGENDIAN */

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
/* #undef YYTEXT_POINTER */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define curl_free() as free() if our version of curl lacks curl_free. */
/* #undef curl_free */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
#define inline __inline
/* #undef inline */
#endif

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define strcasecmp as stricmp if you have one but not the other */
#define strcasecmp stricmp

/* new stuff, for VC2005 compatibility (phrosty) */

#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NONSTDC_NO_WARNINGS

#define DIRECTINPUT_VERSION 0x0600
#define INITGUID

#define snprintf _snprintf
#define strncasecmp strnicmp

#define ssize_t ptrdiff_t
#define S_ISDIR(mode) (mode & _S_IFDIR)

/* used in access() */
#define R_OK 04

/*
	disable silent conversion warnings for fixing later..

	4047: 'operator' : 'identifier1' differs in levels of indirection from 'identifier2'
	4244: 'argument' : conversion from 'type1' to 'type2', possible loss of data
	4267: 'var' : conversion from 'size_t' to 'type', possible loss of data (/Wp64 warning)
	4305: 'identifier' : truncation from 'type1' to 'type2'
	4311: 'variable' : pointer truncation from 'type' to 'type' (/Wp64 warning)
	4312: 'operation' : conversion from 'type1' to 'type2' of greater size (/Wp64 warning)
*/
//#pragma warning(disable:4047 4244 4267 4305 4311 4312)
#pragma warning(disable:4047 4244 4305)
