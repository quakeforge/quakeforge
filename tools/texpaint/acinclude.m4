# Configure paths for GTK+
# Owen Taylor     97-11-3

dnl AM_PATH_GTK([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, MODULES]]]])
dnl Test for GTK, and define GTK_CFLAGS and GTK_LIBS
dnl
AC_DEFUN(AM_PATH_GTK,
[dnl 
dnl Get the cflags and libraries from the gtk-config script
dnl
AC_ARG_WITH(gtk-prefix,[  --with-gtk-prefix=PFX   Prefix where GTK is installed (optional)],
            gtk_config_prefix="$withval", gtk_config_prefix="")
AC_ARG_WITH(gtk-exec-prefix,[  --with-gtk-exec-prefix=PFX Exec prefix where GTK is installed (optional)],
            gtk_config_exec_prefix="$withval", gtk_config_exec_prefix="")
AC_ARG_ENABLE(gtktest, [  --disable-gtktest       Do not try to compile and run a test GTK program],
		    , enable_gtktest=yes)

  for module in . $4
  do
      case "$module" in
         gthread) 
             gtk_config_args="$gtk_config_args gthread"
         ;;
      esac
  done

  if test x$gtk_config_exec_prefix != x ; then
     gtk_config_args="$gtk_config_args --exec-prefix=$gtk_config_exec_prefix"
     if test x${GTK_CONFIG+set} != xset ; then
        GTK_CONFIG=$gtk_config_exec_prefix/bin/gtk-config
     fi
  fi
  if test x$gtk_config_prefix != x ; then
     gtk_config_args="$gtk_config_args --prefix=$gtk_config_prefix"
     if test x${GTK_CONFIG+set} != xset ; then
        GTK_CONFIG=$gtk_config_prefix/bin/gtk-config
     fi
  fi

  AC_PATH_PROG(GTK_CONFIG, gtk-config, no)
  min_gtk_version=ifelse([$1], ,0.99.7,$1)
  AC_MSG_CHECKING(for GTK - version >= $min_gtk_version)
  no_gtk=""
  if test "$GTK_CONFIG" = "no" ; then
    no_gtk=yes
  else
    GTK_CFLAGS=`$GTK_CONFIG $gtk_config_args --cflags`
    GTK_LIBS=`$GTK_CONFIG $gtk_config_args --libs`
    gtk_config_major_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    gtk_config_minor_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    gtk_config_micro_version=`$GTK_CONFIG $gtk_config_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_gtktest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GTK_CFLAGS"
      LIBS="$GTK_LIBS $LIBS"
dnl
dnl Now check if the installed GTK is sufficiently new. (Also sanity
dnl checks the results of gtk-config to some extent
dnl
      rm -f conf.gtktest
      AC_TRY_RUN([
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>

int 
main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.gtktest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup("$min_gtk_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_gtk_version");
     exit(1);
   }

  if ((gtk_major_version != $gtk_config_major_version) ||
      (gtk_minor_version != $gtk_config_minor_version) ||
      (gtk_micro_version != $gtk_config_micro_version))
    {
      printf("\n*** 'gtk-config --version' returned %d.%d.%d, but GTK+ (%d.%d.%d)\n", 
             $gtk_config_major_version, $gtk_config_minor_version, $gtk_config_micro_version,
             gtk_major_version, gtk_minor_version, gtk_micro_version);
      printf ("*** was found! If gtk-config was correct, then it is best\n");
      printf ("*** to remove the old version of GTK+. You may also be able to fix the error\n");
      printf("*** by modifying your LD_LIBRARY_PATH enviroment variable, or by editing\n");
      printf("*** /etc/ld.so.conf. Make sure you have run ldconfig if that is\n");
      printf("*** required on your system.\n");
      printf("*** If gtk-config was wrong, set the environment variable GTK_CONFIG\n");
      printf("*** to point to the correct copy of gtk-config, and remove the file config.cache\n");
      printf("*** before re-running configure\n");
    } 
#if defined (GTK_MAJOR_VERSION) && defined (GTK_MINOR_VERSION) && defined (GTK_MICRO_VERSION)
  else if ((gtk_major_version != GTK_MAJOR_VERSION) ||
	   (gtk_minor_version != GTK_MINOR_VERSION) ||
           (gtk_micro_version != GTK_MICRO_VERSION))
    {
      printf("*** GTK+ header files (version %d.%d.%d) do not match\n",
	     GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION);
      printf("*** library (version %d.%d.%d)\n",
	     gtk_major_version, gtk_minor_version, gtk_micro_version);
    }
#endif /* defined (GTK_MAJOR_VERSION) ... */
  else
    {
      if ((gtk_major_version > major) ||
        ((gtk_major_version == major) && (gtk_minor_version > minor)) ||
        ((gtk_major_version == major) && (gtk_minor_version == minor) && (gtk_micro_version >= micro)))
      {
        return 0;
       }
     else
      {
        printf("\n*** An old version of GTK+ (%d.%d.%d) was found.\n",
               gtk_major_version, gtk_minor_version, gtk_micro_version);
        printf("*** You need a version of GTK+ newer than %d.%d.%d. The latest version of\n",
	       major, minor, micro);
        printf("*** GTK+ is always available from ftp://ftp.gtk.org.\n");
        printf("***\n");
        printf("*** If you have already installed a sufficiently new version, this error\n");
        printf("*** probably means that the wrong copy of the gtk-config shell script is\n");
        printf("*** being found. The easiest way to fix this is to remove the old version\n");
        printf("*** of GTK+, but you can also set the GTK_CONFIG environment to point to the\n");
        printf("*** correct copy of gtk-config. (In this case, you will have to\n");
        printf("*** modify your LD_LIBRARY_PATH enviroment variable, or edit /etc/ld.so.conf\n");
        printf("*** so that the correct libraries are found at run-time))\n");
      }
    }
  return 1;
}
],, no_gtk=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_gtk" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GTK_CONFIG" = "no" ; then
       echo "*** The gtk-config script installed by GTK could not be found"
       echo "*** If GTK was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GTK_CONFIG environment variable to the"
       echo "*** full path to gtk-config."
     else
       if test -f conf.gtktest ; then
        :
       else
          echo "*** Could not run GTK test program, checking why..."
          CFLAGS="$CFLAGS $GTK_CFLAGS"
          LIBS="$LIBS $GTK_LIBS"
          AC_TRY_LINK([
#include <gtk/gtk.h>
#include <stdio.h>
],      [ return ((gtk_major_version) || (gtk_minor_version) || (gtk_micro_version)); ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GTK or finding the wrong"
          echo "*** version of GTK. If it is not finding GTK, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"
          echo "***"
          echo "*** If you have a RedHat 5.0 system, you should remove the GTK package that"
          echo "*** came with the system with the command"
          echo "***"
          echo "***    rpm --erase --nodeps gtk gtk-devel" ],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GTK was incorrectly installed"
          echo "*** or that you have moved GTK since it was installed. In the latter case, you"
          echo "*** may want to edit the gtk-config script: $GTK_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GTK_CFLAGS=""
     GTK_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GTK_CFLAGS)
  AC_SUBST(GTK_LIBS)
  rm -f conf.gtktest
])

# Configure paths for GIMP
# Manish Singh    98-6-11
# Shamelessly stolen from Owen Taylor

dnl AM_PATH_GIMP([MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for GIMP, and define GIMP_CFLAGS and GIMP_LIBS
dnl
AC_DEFUN(AM_PATH_GIMP,
[dnl 
dnl Get the cflags and libraries from the gimptool script
dnl
AC_ARG_WITH(gimp-prefix,[  --with-gimp-prefix=PFX  Prefix where GIMP is installed (optional)],
            gimptool_prefix="$withval", gimptool_prefix="")
AC_ARG_WITH(gimp-exec-prefix,[  --with-gimp-exec-prefix=PFX Exec prefix where GIMP is installed (optional)],
            gimptool_exec_prefix="$withval", gimptool_exec_prefix="")
AC_ARG_ENABLE(gimptest, [  --disable-gimptest      Do not try to compile and run a test GIMP program],
		    , enable_gimptest=yes)

  if test x$gimptool_exec_prefix != x ; then
     gimptool_args="$gimptool_args --exec-prefix=$gimptool_exec_prefix"
     if test x${GIMPTOOL+set} != xset ; then
        GIMPTOOL=$gimptool_exec_prefix/bin/gimptool
     fi
  fi
  if test x$gimptool_prefix != x ; then
     gimptool_args="$gimptool_args --prefix=$gimptool_prefix"
     if test x${GIMPTOOL+set} != xset ; then
        GIMPTOOL=$gimptool_prefix/bin/gimptool
     fi
  fi

  AC_PATH_PROG(GIMPTOOL, gimptool, no)
  min_gimp_version=ifelse([$1], ,1.0.0,$1)
  AC_MSG_CHECKING(for GIMP - version >= $min_gimp_version)
  no_gimp=""
  if test "$GIMPTOOL" = "no" ; then
    no_gimp=yes
  else
    GIMP_CFLAGS=`$GIMPTOOL $gimptool_args --cflags`
    GIMP_LIBS=`$GIMPTOOL $gimptool_args --libs`

    GIMP_CFLAGS_NOUI=`$GIMPTOOL $gimptool_args --cflags-noui`
    noui_test=`echo $GIMP_CFLAGS_NOUI | sed 's/^\(Usage\).*/\1/'`
    if test "$noui_test" = "Usage" ; then
       GIMP_CFLAGS_NOUI=$GIMP_CFLAGS
       GIMP_LIBS_NOUI=$GIMP_LIBS
    else
       GIMP_LIBS_NOUI=`$GIMPTOOL $gimptool_args --libs-noui`
    fi

    GIMP_DATA_DIR=`$GIMPTOOL $gimptool_args --gimpdatadir`
    GIMP_PLUGIN_DIR=`$GIMPTOOL $gimptool_args --gimpplugindir`
    nodatadir_test=`echo $GIMP_DATA_DIR | sed 's/^\(Usage\).*/\1/'`
    if test "$nodatadir_test" = "Usage" ; then
       GIMP_DATA_DIR=""
       GIMP_PLUGIN_DIR=""
    fi

    gimptool_major_version=`$GIMPTOOL $gimptool_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\1/'`
    gimptool_minor_version=`$GIMPTOOL $gimptool_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\2/'`
    gimptool_micro_version=`$GIMPTOOL $gimptool_args --version | \
           sed 's/\([[0-9]]*\).\([[0-9]]*\).\([[0-9]]*\)/\3/'`
    if test "x$enable_gimptest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GIMP_CFLAGS"
      LIBS="$LIBS $GIMP_LIBS"
dnl
dnl Now check if the installed GIMP is sufficiently new. (Also sanity
dnl checks the results of gimptool to some extent
dnl
      rm -f conf.gimptest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>

#include <libgimp/gimp.h>

#ifndef GIMP_CHECK_VERSION
#define GIMP_CHECK_VERSION(major, minor, micro) \
    (GIMP_MAJOR_VERSION > (major) || \
     (GIMP_MAJOR_VERSION == (major) && GIMP_MINOR_VERSION > (minor)) || \
     (GIMP_MAJOR_VERSION == (major) && GIMP_MINOR_VERSION == (minor) && \
      GIMP_MICRO_VERSION >= (micro)))
#endif

#if GIMP_CHECK_VERSION(1,1,24)
GimpPlugInInfo
#else
GPlugInInfo
#endif
PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  NULL,  /* query_proc */
  NULL   /* run_proc */
};

int main ()
{
  int major, minor, micro;
  char *tmp_version;

  system ("touch conf.gimptest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = g_strdup("$min_gimp_version");
  if (sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) != 3) {
     printf("%s, bad version string\n", "$min_gimp_version");
     exit(1);
   }

    if (($gimptool_major_version > major) ||
        (($gimptool_major_version == major) && ($gimptool_minor_version > minor)) ||
        (($gimptool_major_version == major) && ($gimptool_minor_version == minor) && ($gimptool_micro_version >= micro)))
    {
      return 0;
    }
  else
    {
      printf("\n*** 'gimptool --version' returned %d.%d.%d, but the minimum version\n", $gimptool_major_version, $gimptool_minor_version, $gimptool_micro_version);
      printf("*** of GIMP required is %d.%d.%d. If gimptool is correct, then it is\n", major, minor, micro);
      printf("*** best to upgrade to the required version.\n");
      printf("*** If gimptool was wrong, set the environment variable GIMPTOOL\n");
      printf("*** to point to the correct copy of gimptool, and remove the file\n");
      printf("*** config.cache before re-running configure\n");
      return 1;
    }
}

],, no_gimp=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_gimp" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GIMPTOOL" = "no" ; then
       echo "*** The gimptool script installed by GIMP could not be found"
       echo "*** If GIMP was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GIMPTOOL environment variable to the"
       echo "*** full path to gimptool."
     else
       if test -f conf.gimptest ; then
        :
       else
          echo "*** Could not run GIMP test program, checking why..."
          CFLAGS="$CFLAGS $GIMP_CFLAGS"
          LIBS="$LIBS $GIMP_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <libgimp/gimp.h>

#ifndef GIMP_CHECK_VERSION
#define GIMP_CHECK_VERSION(major, minor, micro) \
    (GIMP_MAJOR_VERSION > (major) || \
     (GIMP_MAJOR_VERSION == (major) && GIMP_MINOR_VERSION > (minor)) || \
     (GIMP_MAJOR_VERSION == (major) && GIMP_MINOR_VERSION == (minor) && \
      GIMP_MICRO_VERSION >= (micro)))
#endif

#if GIMP_CHECK_VERSION(1,1,24)
GimpPlugInInfo
#else
GPlugInInfo
#endif
PLUG_IN_INFO =
{
  NULL,  /* init_proc */
  NULL,  /* quit_proc */
  NULL,  /* query_proc */
  NULL   /* run_proc */
};
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GIMP or finding the wrong"
          echo "*** version of GIMP. If it is not finding GIMP, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GIMP was incorrectly installed"
          echo "*** or that you have moved GIMP since it was installed. In the latter case, you"
          echo "*** may want to edit the gimptool script: $GIMPTOOL" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     GIMP_CFLAGS=""
     GIMP_LIBS=""
     GIMP_CFLAGS_NOUI=""
     GIMP_LIBS_NOUI=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GIMP_CFLAGS)
  AC_SUBST(GIMP_LIBS)
  AC_SUBST(GIMP_CFLAGS_NOUI)
  AC_SUBST(GIMP_LIBS_NOUI)
  AC_SUBST(GIMP_DATA_DIR)
  AC_SUBST(GIMP_PLUGIN_DIR)
  rm -f conf.gimptest
])

