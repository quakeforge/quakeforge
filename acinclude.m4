dnl check for fields in a structure
dnl
dnl AC_HAVE_STRUCT_FIELD(struct, field, headers)

AC_DEFUN(AC_HAVE_STRUCT_FIELD, [
define(cache_val, translit(ac_cv_type_$1_$2, [A-Z ], [a-z_]))
AC_CACHE_CHECK([for $2 in $1], cache_val,[
AC_TRY_COMPILE([$3],[$1 x; x.$2;],
cache_val=yes,
cache_val=no)])
if test "$cache_val" = yes; then
	define(foo, translit(HAVE_$1_$2, [a-z ], [A-Z_]))
	AC_DEFINE(foo, 1, [Define if $1 has field $2.])
	undefine(foo)
fi
undefine(cache_val)
])
# Configure paths for SDL
# Sam Lantinga 9/21/99
# stolen from Manish Singh
# stolen back from Frank Belew
# stolen from Manish Singh
# Shamelessly stolen from Owen Taylor

dnl AM_PATH_SDL([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for SDL, and define SDL_CFLAGS and SDL_LIBS
dnl
AC_DEFUN(AM_PATH_SDL,
[dnl 
dnl Get the cflags and libraries from the sdl-config script
dnl
AC_ARG_WITH(sdl-prefix,[  --with-sdl-prefix=PFX   Prefix where SDL is installed (optional)],
            sdl_prefix="$withval", sdl_prefix="")
AC_ARG_WITH(sdl-exec-prefix,[  --with-sdl-exec-prefix=PFX Exec prefix where SDL is installed (optional)],
            sdl_exec_prefix="$withval", sdl_exec_prefix="")
AC_ARG_ENABLE(sdltest, [  --disable-sdltest       Do not try to compile and run a test SDL program],
		    , enable_sdltest=yes)

  if test x$sdl_exec_prefix != x ; then
     sdl_args="$sdl_args --exec-prefix=$sdl_exec_prefix"
     if test x${SDL_CONFIG+set} != xset ; then
        SDL_CONFIG=$sdl_exec_prefix/bin/sdl-config
     fi
  fi
  if test x$sdl_prefix != x ; then
     sdl_args="$sdl_args --prefix=$sdl_prefix"
     if test x${SDL_CONFIG+set} != xset ; then
        SDL_CONFIG=$sdl_prefix/bin/sdl-config
     fi
  fi

  AC_PATH_PROG(SDL_CONFIG, sdl-config, no)
  min_sdl_version=ifelse([$1], ,0.11.0,$1)
  AC_MSG_CHECKING(for SDL - version >= $min_sdl_version)
  no_sdl=""
  if test "$SDL_CONFIG" = "no" ; then
    no_sdl=yes
  else
    SDL_CFLAGS=`$SDL_CONFIG $sdlconf_args --cflags`
    SDL_LIBS=`$SDL_CONFIG $sdlconf_args --libs`

    sdl_major_version=`$SDL_CONFIG $sdl_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    sdl_minor_version=`$SDL_CONFIG $sdl_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    sdl_micro_version=`$SDL_CONFIG $sdl_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_sdltest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $SDL_CFLAGS"
      LIBS="$SDL_LIBS"
dnl
dnl Now check if the installed SDL is sufficiently new. (Also sanity
dnl checks the results of sdl-config to some extent
dnl
      rm -f conf.sdltest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>

char*
my_strdup (char *str)
{
  char *new_str;
  
  if (str)
    {
      new_str = malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
  else
    new_str = NULL;
  
  return new_str;
}

int main (int argc, char *argv[])
{
  int major, minor, micro;
  char *tmp_version;

  /* This hangs on some systems (?)
  system ("touch conf.sdltest");
  */
  { FILE *fp = fopen("conf.sdltest", "a"); if ( fp ) fclose(fp); }

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_sdl_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_sdl_version");
     exit(1);
   }

   if (($sdl_major_version > major) ||
      (($sdl_major_version == major) && ($sdl_minor_version > minor)) ||
      (($sdl_major_version == major) && ($sdl_minor_version == minor) && ($sdl_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'sdl-config --version' returned %d.%d.%d, but the minimum version\n", $sdl_major_version, $sdl_minor_version, $sdl_micro_version);
      printf("*** of SDL required is %d.%d.%d. If sdl-config is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If sdl-config was wrong, set the environment variable SDL_CONFIG\n");
      printf("*** to point to the correct copy of sdl-config, and remove the file\n");
      printf("*** config.cache before re-running configure\n");
      return 1;
    }
}

],, no_sdl=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_sdl" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$SDL_CONFIG" = "no" ; then
       echo "*** The sdl-config script installed by SDL could not be found"
       echo "*** If SDL was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the SDL_CONFIG environment variable to the"
       echo "*** full path to sdl-config."
     else
       if test -f conf.sdltest ; then
        :
       else
          echo "*** Could not run SDL test program, checking why..."
          CFLAGS="$CFLAGS $SDL_CFLAGS"
          LIBS="$LIBS $SDL_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <SDL.h>
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding SDL or finding the wrong"
          echo "*** version of SDL. If it is not finding SDL, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means SDL was incorrectly installed"
          echo "*** or that you have moved SDL since it was installed. In the latter case, you"
          echo "*** may want to edit the sdl-config script: $SDL_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     SDL_CFLAGS=""
     SDL_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(SDL_CFLAGS)
  AC_SUBST(SDL_LIBS)
  rm -f conf.sdltest
])

dnl AM_PROG_LEX
dnl Look for flex, lex or missing, then run AC_PROG_LEX and AC_DECL_YYTEXT
AC_DEFUN(AM_PROG_LEX,
[missing_dir=ifelse([$1],,`cd $ac_aux_dir && pwd`,$1)
AC_CHECK_PROGS(LEX, flex lex, $missing_dir/missing flex)
AC_PROG_LEX
AC_DECL_YYTEXT
])
# CFLAGS and library paths for XMMS
# written 15 December 1999 by Ben Gertzfield <che@debian.org>

dnl Usage:
dnl AM_PATH_XMMS([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl
dnl Example:
dnl AM_PATH_XMMS(0.9.5.1, , AC_MSG_ERROR([*** XMMS >= 0.9.5.1 not installed - please install first ***]))
dnl
dnl Defines XMMS_CFLAGS, XMMS_LIBS, XMMS_DATA_DIR, XMMS_PLUGIN_DIR, 
dnl XMMS_VISUALIZATION_PLUGIN_DIR, XMMS_INPUT_PLUGIN_DIR, 
dnl XMMS_OUTPUT_PLUGIN_DIR, XMMS_GENERAL_PLUGIN_DIR, XMMS_EFFECT_PLUGIN_DIR,
dnl and XMMS_VERSION for your plugin pleasure.
dnl

dnl XMMS_TEST_VERSION(AVAILABLE-VERSION, NEEDED-VERSION [, ACTION-IF-OKAY [, ACTION-IF-NOT-OKAY]])
AC_DEFUN(XMMS_TEST_VERSION, [

# Determine which version number is greater. Prints 2 to stdout if	
# the second number is greater, 1 if the first number is greater,	
# 0 if the numbers are equal.						
									
# Written 15 December 1999 by Ben Gertzfield <che@debian.org>		
# Revised 15 December 1999 by Jim Monty <monty@primenet.com>		
									
    AC_PROG_AWK
    xmms_got_version=[` $AWK '						\
BEGIN {									\
    print vercmp(ARGV[1], ARGV[2]);					\
}									\
									\
function vercmp(ver1, ver2,    ver1arr, ver2arr,			\
                               ver1len, ver2len,			\
                               ver1int, ver2int, len, i, p) {		\
									\
    ver1len = split(ver1, ver1arr, /\./);				\
    ver2len = split(ver2, ver2arr, /\./);				\
									\
    len = ver1len > ver2len ? ver1len : ver2len;			\
									\
    for (i = 1; i <= len; i++) {					\
        p = 1000 ^ (len - i);						\
        ver1int += ver1arr[i] * p;					\
        ver2int += ver2arr[i] * p;					\
    }									\
									\
    if (ver1int < ver2int)						\
        return 2;							\
    else if (ver1int > ver2int)						\
        return 1;							\
    else								\
        return 0;							\
}' $1 $2`]								

    if test $xmms_got_version -eq 2; then 	# failure
	ifelse([$4], , :, $4)			
    else  					# success!
	ifelse([$3], , :, $3)
    fi
])

AC_DEFUN(AM_PATH_XMMS,
[
AC_ARG_WITH(xmms-prefix,[  --with-xmms-prefix=PFX  Prefix where XMMS is installed (optional)],
	xmms_config_prefix="$withval", xmms_config_prefix="")
AC_ARG_WITH(xmms-exec-prefix,[  --with-xmms-exec-prefix=PFX Exec prefix where XMMS is installed (optional)],
	xmms_config_exec_prefix="$withval", xmms_config_exec_prefix="")

if test x$xmms_config_exec_prefix != x; then
    xmms_config_args="$xmms_config_args --exec-prefix=$xmms_config_exec_prefix"
    if test x${XMMS_CONFIG+set} != xset; then
	XMMS_CONFIG=$xmms_config_exec_prefix/bin/xmms-config
    fi
fi

if test x$xmms_config_prefix != x; then
    xmms_config_args="$xmms_config_args --prefix=$xmms_config_prefix"
    if test x${XMMS_CONFIG+set} != xset; then
  	XMMS_CONFIG=$xmms_config_prefix/bin/xmms-config
    fi
fi

AC_PATH_PROG(XMMS_CONFIG, xmms-config, no)
min_xmms_version=ifelse([$1], ,0.9.5.1, $1)

if test "$XMMS_CONFIG" = "no"; then
    no_xmms=yes
else
    XMMS_CFLAGS=`$XMMS_CONFIG $xmms_config_args --cflags`
    XMMS_LIBS=`$XMMS_CONFIG $xmms_config_args --libs`
    XMMS_VERSION=`$XMMS_CONFIG $xmms_config_args --version`
    XMMS_DATA_DIR=`$XMMS_CONFIG $xmms_config_args --data-dir`
    XMMS_PLUGIN_DIR=`$XMMS_CONFIG $xmms_config_args --plugin-dir`
    XMMS_VISUALIZATION_PLUGIN_DIR=`$XMMS_CONFIG $xmms_config_args \
					--visualization-plugin-dir`
    XMMS_INPUT_PLUGIN_DIR=`$XMMS_CONFIG $xmms_config_args --input-plugin-dir`
    XMMS_OUTPUT_PLUGIN_DIR=`$XMMS_CONFIG $xmms_config_args --output-plugin-dir`
    XMMS_EFFECT_PLUGIN_DIR=`$XMMS_CONFIG $xmms_config_args --effect-plugin-dir`
    XMMS_GENERAL_PLUGIN_DIR=`$XMMS_CONFIG $xmms_config_args --general-plugin-dir`

    XMMS_TEST_VERSION($XMMS_VERSION, $min_xmms_version, ,no_xmms=version)
fi

AC_MSG_CHECKING(for XMMS - version >= $min_xmms_version)

if test "x$no_xmms" = x; then
    AC_MSG_RESULT(yes)
    ifelse([$2], , :, [$2])
else
    AC_MSG_RESULT(no)

    if test "$XMMS_CONFIG" = "no" ; then
	echo "*** The xmms-config script installed by XMMS could not be found."
      	echo "*** If XMMS was installed in PREFIX, make sure PREFIX/bin is in"
	echo "*** your path, or set the XMMS_CONFIG environment variable to the"
	echo "*** full path to xmms-config."
    else
	if test "$no_xmms" = "version"; then
	    echo "*** An old version of XMMS, $XMMS_VERSION, was found."
	    echo "*** You need a version of XMMS newer than $min_xmms_version."
	    echo "*** The latest version of XMMS is always available from"
	    echo "*** http://www.xmms.org/"
	    echo "***"

            echo "*** If you have already installed a sufficiently new version, this error"
            echo "*** probably means that the wrong copy of the xmms-config shell script is"
            echo "*** being found. The easiest way to fix this is to remove the old version"
            echo "*** of XMMS, but you can also set the XMMS_CONFIG environment to point to the"
            echo "*** correct copy of xmms-config. (In this case, you will have to"
            echo "*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf"
            echo "*** so that the correct libraries are found at run-time)"
	fi
    fi
    XMMS_CFLAGS=""
    XMMS_LIBS=""
    ifelse([$3], , :, [$3])
fi
AC_SUBST(XMMS_CFLAGS)
AC_SUBST(XMMS_LIBS)
AC_SUBST(XMMS_VERSION)
AC_SUBST(XMMS_DATA_DIR)
AC_SUBST(XMMS_PLUGIN_DIR)
AC_SUBST(XMMS_VISUALIZATION_PLUGIN_DIR)
AC_SUBST(XMMS_INPUT_PLUGIN_DIR)
AC_SUBST(XMMS_OUTPUT_PLUGIN_DIR)
AC_SUBST(XMMS_GENERAL_PLUGIN_DIR)
AC_SUBST(XMMS_EFFECT_PLUGIN_DIR)
])
