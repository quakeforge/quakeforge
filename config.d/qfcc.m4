dnl silence automake about .r files
F77=touch
AC_SUBST(F77)

AC_ARG_WITH(cpp,
	AS_HELP_STRING([--with-cpp=CPP], [how qfcc should invoke cpp]),
	cpp_name="$withval", cpp_name=auto)
if test "x$cpp_name" != xauto; then
	CPP_NAME="$cpp_name"
else
	CPP_NAME="cpp %u %d %s -o %o %i"
	case "$host_os" in
		*freebsd*)
			CPP_NAME="cpp %d %i %o"
			;;
		*openbsd*)
			# /usr/bin/cpp is a wrapper script to /usr/libexec/cpp.
			# It parses incorrectly the options needed, so call the
			# binary directly.
			CPP_NAME="/usr/libexec/cpp %d -o %o %i"
			;;
		*qnx*)
			CPP_NAME="gcc -E -x c++ %d -o %o %i"
			;;
		*darwin*)
			CPP_NAME="/usr/libexec/gcc/darwin/`/usr/bin/arch`/default/cpp %d -o %o %i"
			;;
		*bsd*)
			touch conftest.c
			CPP_NAME="`(f=\`$CC -v -E -Dfoo conftest.c -o conftest.i 2>&1 | grep -e -Dfoo\`; set $f; echo "$1")` %d %i %o"
			rm -f conftest.[ci]
			;;
	esac
fi

AC_DEFINE_UNQUOTED(PATH_SEPARATOR, '/',
	[Define this to your operating system path separator character])
AC_DEFINE_UNQUOTED(CPP_NAME, "$CPP_NAME",
	[Define this to the command line for the C preprocessor])
eval expanded_datarootdir="${datarootdir}"
eval expanded_datarootdir="${expanded_datarootdir}"
AC_DEFINE_UNQUOTED(QFCC_INCLUDE_PATH, "${expanded_datarootdir}/qfcc/include",
	[Define this to where qfcc should look for header files])
AC_DEFINE_UNQUOTED(QFCC_LIB_PATH, "${expanded_datarootdir}/qfcc/lib",
	[Define this to where qfcc should look for lib files])
