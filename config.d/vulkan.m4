dnl Check for vulkan support
AC_ARG_ENABLE(vulkan,
	AS_HELP_STRING([--disable-vulkan], [do not use Vulkan]),
	HAVE_VULKAN=$enable_vulkan, HAVE_VULKAN=auto)
if test "x$HAVE_VULKAN" != xno; then
	save_CPPFLAGS="$CPPFLAGS"
	AS_IF([test x"$VULKAN_SDK" != x], [
		CPPFLAGS="$CPPFLAGS -I$VULKAN_SDK/include"
		LDFLAGS="$LDFLAGS -L$VULKAN_SDK/lib"
		glslangvalidator="$VULKAN_SDK/bin/glslangValidator"
	  ], [glslangvalidator="glslangValidator"])
	AC_CHECK_HEADER([vulkan/vulkan.h], [HAVE_VULKAN=yes], [HAVE_VULKAN=no])
	CPPFLAGS="$save_CPPFLAGS"
fi
if test "x$HAVE_VULKAN" = xyes; then
	AC_DEFINE([HAVE_VULKAN], [1], [Define if yhou have the Vulkan libs])
fi
AC_SUBST(VULKAN_LIBS)
AC_SUBST(GLSLANGVALIDATOR, [$glslangvalidator])

AM_CONDITIONAL(X11_VULKAN, test "x$HAVE_VULKAN" = "xyes")
AM_CONDITIONAL(WIN_VULKAN, test "x$HAVE_VULKAN" = "xyes")
AM_CONDITIONAL(TEST_VULKAN, test "x$HAVE_VULKAN" = "xyes")
