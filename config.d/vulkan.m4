dnl Check for vulkan support
AC_ARG_ENABLE(vulkan,
[  --disable-vulkan        do not use Vulkan],
	HAVE_VULKAN=$enable_vulkan, HAVE_VULKAN=auto)
if test "x$HAVE_VULKAN" != xno; then
	save_CPPFLAGS="$CPPFLAGS"
	AS_IF([test x"$VULKAN_SDK" != x], [
		CPPFLAGS="$CPPFLAGS -I$VULKAN_SDK/include"
		LDFLAGS="$LDFLAGS -L$VULKAN_SDK/lib"
		glslangvalidator="$VULKAN_SDK/bin/glslangValidator"
	  ], [glslangvalidator="glslangValidator"])
	AC_CHECK_HEADER(
		[vulkan/vulkan.h],
		dnl Make sure the library "works"
		[AC_CHECK_LIB([vulkan], [vkCreateInstance],
			[AC_DEFINE([HAVE_VULKAN], [1], [Define if yhou have the Vulkan libs])
			 VULKAN_LIBS=-lvulkan],
			[AC_MSG_RESULT(no)])
		],
		[AC_MSG_RESULT(no)]
	)
	CPPFLAGS="$save_CPPFLAGS"
fi
AC_SUBST(VULKAN_LIBS)
