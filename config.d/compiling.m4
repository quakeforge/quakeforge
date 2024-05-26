if test -d $srcdir/.git; then
	cvs_def_enabled="!= xno"
	cvs_def_disabled="= xyes"
else
	cvs_def_enabled="= xyes"
	cvs_def_disabled="!= xno"
fi

AC_MSG_CHECKING(for clang)
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[]],
		[[#ifndef __clang__],
		 [	choke me],
		 [#endif]])],
	[CLANG=yes],
	[CLANG=no]
)
AC_MSG_RESULT($CLANG)

AC_MSG_CHECKING(for CFLAGS pre-set)
leave_cflags_alone=no
if test "x$CFLAGS" != "x"; then
	leave_cflags_alone=yes
	BUILD_TYPE="$BUILD_TYPE Custom"
fi
AC_MSG_RESULT([$leave_cflags_alone])

AC_MSG_CHECKING(for C99 inline)
c99_inline=no
AC_LINK_IFELSE(
	[AC_LANG_PROGRAM(
		[[inline int foo (int x) { return x * x; }
		  int (*bar) (int) = foo;]],
		[[]])],
	[c99_inline=no
		AC_MSG_RESULT(no)],
	[c99_inline=yes
		AC_DEFINE(HAVE_C99INLINE, extern, define this if using c99 inline)
		AC_MSG_RESULT(yes)]
)
AH_VERBATIM([HAVE_C99INLINE],
[#undef HAVE_C99INLINE
#ifdef __clang__
# define GNU89INLINE static
#else
# ifdef HAVE_C99INLINE
#  define GNU89INLINE
# else
#  define GNU89INLINE extern
# endif
#endif])

if test "x$GCC" = xyes; then
	set $CC
	shift
	args="$*"
	AC_MSG_CHECKING(for gcc version)
	CCVER="gcc `$CC --version | grep '[[0-9]]\.[[0-9]]' | sed -e 's/.*([[^)]]*)//' -e 's/[[^0-9]]*\([[0-9.]]*\).*/\1/'`"
	set $CCVER
	save_IFS="$IFS"
	IFS="."
	set $2
	CC_MAJ=$1
	CC_MIN=$2
	CC_SUB=$3
	IFS="$save_IFS"
	AC_MSG_RESULT($CCVER)
fi

AC_ARG_ENABLE(debug,
	AS_HELP_STRING([--disable-debug], [compile without debugging]),
	debug=$enable_debug
)

AC_MSG_CHECKING(for debugging)
if test "x$debug" != xno -a "x$leave_cflags_alone" != xyes; then
	AC_MSG_RESULT(yes)
	BUILD_TYPE="$BUILD_TYPE Debug"
	DEBUG=-g
	if test "x$GCC" = xyes; then
		if test "$CC_MAJ" -ge 4; then
			DEBUG=-g3
		else
			if test "$CC_MAJ" -eq 3 -a "$CC_MIN" -ge 1; then
				DEBUG=-g3
			fi
		fi
	fi
	CFLAGS="$CFLAGS $DEBUG"
else
	AC_MSG_RESULT(no)
fi

AC_ARG_ENABLE(Werror,
	AS_HELP_STRING([--disable-Werror], [do not treat warnings as errors]))
dnl We want warnings, lots of warnings...
dnl The help text should be INVERTED before release!
dnl when in git, this test defaults to ENABLED.
dnl In a release, this test defaults to DISABLED.
if test "x$GCC" = "xyes" -a "x$leave_cflags_alone" != xyes; then
	if test "x$enable_Werror" $cvs_def_enabled; then
		CFLAGS="$CFLAGS -Wall -Werror -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations"
	else
		CFLAGS="$CFLAGS -Wall"
	fi
	CFLAGS="$CFLAGS -fno-common"
fi

if test "x$CLANG" = xyes -a "x$leave_cflags_alone" != xyes; then
	CFLAGS="$CFLAGS -Wno-misleading-indentation"
fi

AC_ARG_ENABLE(ubsan,
	AS_HELP_STRING([--enable-ubsan],
		[compile with ubsan (for development)]),
	ubsan=$enable_ubsan,
	ubsan=no
)
if test "x$ubsan" = xyes -a "x$leave_cflags_alone" != "xyes"; then
	QF_CC_OPTION(-fsanitize=undefined)
	QF_CC_OPTION_TEST([-fsanitize=undefined], [
		CFLAGS="$CFLAGS -fsanitize=undefined"
		CXXFLAGS="$CXXFLAGS -fsanitize=undefined"
	])
fi

AC_ARG_ENABLE(optimize,
	AS_HELP_STRING([--disable-optimize],
		[compile without optimizations (for development)]),
	optimize=$enable_optimize,
	optimize=yes
)

if test "x$host_cpu" = xaarch64; then
	simd=neon
else
	AC_ARG_ENABLE(simd,
		AS_HELP_STRING([--enable-simd@<:@=arg@:.@],
			[enable SIMD support (default auto)]),
		[],
		[enable_simd=yes]
	)

	case "$enable_simd" in
		no)
			simd=no
			;;
		sse|sse2|avx|avx2)
			QF_CC_OPTION(-m$enable_simd)
			simd=$enable_simd
			;;
		yes)
			for simd in avx2 avx sse2 sse; do
				if lscpu | grep -q -w $simd; then
					QF_CC_OPTION(-m$simd)
					break
				fi
			done
			;;
	esac
	case "$simd" in
		avx*)
			;;
		*)
			QF_CC_OPTION(-Wno-psabi)
			;;
	esac
fi

AC_MSG_CHECKING(for optimization)
if test "x$optimize" = xyes -a "x$leave_cflags_alone" != "xyes"; then
	AC_MSG_RESULT(yes)
	BUILD_TYPE="$BUILD_TYPE Optimize"
	if test "x$GCC" = xyes; then
		saved_cflags="$CFLAGS"
		dnl CFLAGS=""
		QF_CC_OPTION(-frename-registers)
		QF_CC_OPTION(-fexpensive-optimizations)
		dnl if test "$CC_MAJ" -ge 4; then
		dnl 	QF_CC_OPTION(-finline-limit=32000 -Winline)
		dnl fi
		dnl heavy="-O2 -ffast-math -fno-unsafe-math-optimizations -funroll-loops -fomit-frame-pointer"
		heavy="-O2 -fno-fast-math -funroll-loops -fomit-frame-pointer "
		CFLAGS="$saved_cflags"
		light="-O2"
		AC_ARG_ENABLE(strict-aliasing,
			AS_HELP_STRING([--enable-strict-aliasing],
				[enable the -fstrict-aliasing option of gcc]))
		if test "x$enable_strict_aliasing" = "xyes"; then
			heavy="$heavy -fstrict-aliasing"
			light="$light -fstrict-aliasing"
		else
			if test "x$enable_strict_aliasing" != "xno"; then
				if test $CC_MAJ -ge 3; then
					heavy="$heavy -fstrict-aliasing"
				else
					if test $CC_MAJ = 2 -a $CC_MIN = 96; then
						light="$light -fno-strict-aliasing"
					fi
				fi
			else
				if test $CC_MAJ = 2 -a $CC_MIN = 96; then
					light="$light -fno-strict-aliasing"
				fi
			fi
		fi
		AC_MSG_CHECKING(for special compiler settings)
		AC_ARG_WITH(arch,
			AS_HELP_STRING([--with-arch=ARCH],
				[control compiler architecture directly]),
			arch="$withval", arch=auto
		)
		case "$arch" in
			auto)
				case "${host_cpu}" in
					i?86)
						MORE_CFLAGS="-march=${host_cpu}"
						;;
					*)
						MORE_CFLAGS=""
						;;
				esac
				;;
			no|"")
				MORE_CFLAGS=""
				;;
			*)
				MORE_CFLAGS="-march=$arch"
				;;
		esac
		if test "x$MORE_CFLAGS" = x; then
			AC_MSG_RESULT(no)
		else
			save_CFLAGS="$CFLAGS"
			CFLAGS="$CFLAGS $MORE_CFLAGS"
			AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[AC_MSG_RESULT(yes)],[CFLAGS="$save_CFLAGS"
				AC_MSG_RESULT(no)
			])
		fi
		if test $CC_MAJ = 2 -a $CC_MIN = 96; then
			AC_MSG_CHECKING(if align options work)
			save_CFLAGS="$CFLAGS"
			CFLAGS="$CFLAGS -malign-loops=2 -malign-jumps=2 -malign-functions=2"
			AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[]])],[light="$light -malign-loops=2 -malign-jumps=2 -malign-functions=2"
				AC_MSG_RESULT(yes)],[AC_MSG_RESULT(no)
			])
			CFLAGS="$save_CFLAGS"
			CFLAGS="$CFLAGS $light"
		else
			CFLAGS="$CFLAGS $heavy"
		fi
	else
		CFLAGS=-O2
	fi
else
	AC_MSG_RESULT(no)
fi

dnl CFLAGS for release and devel versions
AC_ARG_ENABLE(profile,
	AS_HELP_STRING([--enable-profile],
		[compile with profiling (for development)]),
	profile=$enable_profile
)
if test "x$profile" = xyes -a "x$leave_cflags_alone" != xyes; then
	BUILD_TYPE="$BUILD_TYPE Profile"
	if test "x$GCC" = xyes; then
		CFLAGS="`echo $CFLAGS | sed -e 's/-fomit-frame-pointer//g'` -pg"
		LDFLAGS="$LDFLAGS -pg"
	else
		CFLAGS="$CFLAGS -p"
	fi
fi

check_pipe=no
if test "x$GCC" = xyes -a "x$leave_cflags_alone" != xyes; then
	dnl Check for -pipe vs -save-temps.
	AC_MSG_CHECKING(for -pipe vs -save-temps)
	AC_ARG_ENABLE(save-temps,
		AS_HELP_STRING([--enable-save-temps], [save temporary files]),
		AC_MSG_RESULT(-save-temps)
		CFLAGS="$CFLAGS -save-temps"
		BUILD_TYPE="$BUILD_TYPE Save-temps"
		,
		AC_MSG_RESULT(-pipe)
		check_pipe=yes
	)
fi
if test "x$check_pipe" = xyes; then
	QF_CC_OPTION(-pipe)
fi
QF_CC_OPTION(-Wsign-compare)
if test $CC_MAJ -gt 4 -o $CC_MAJ -eq 4 -a $CC_MIN -ge 5; then
	QF_CC_OPTION(-Wlogical-op)
fi
QF_CC_OPTION(-Wtype-limits)
QF_CC_OPTION_TEST([-fvisibility=hidden], [VISIBILITY=-fvisibility=hidden])
QF_CC_OPTION(-Wsuggest-attribute=pure)
QF_CC_OPTION(-Wsuggest-attribute=const)
QF_CC_OPTION(-Wsuggest-attribute=noreturn)
QF_CC_OPTION(-Wsuggest-attribute=format)
QF_CC_OPTION(-Wformat-nonliteral)

AC_ARG_ENABLE(coverage,
	AS_HELP_STRING([--enable-coverage], [enable generation of data for gcov]))
if test "x$enable_coverage" = xyes; then
	QF_CC_OPTION(-fprofile-arcs -ftest-coverage)
fi

dnl QuakeForge uses lots of BCPL-style (//) comments, which can cause problems
dnl with many compilers that do not support the latest ISO standards. Well,
dnl that is our cover story -- the reality is that we like them and do not want
dnl to give them up. :)
dnl Make the compiler swallow its pride...
if test "x$GCC" != xyes -a "x$CLANG" != xyes -a "x$leave_cflags_alone" != xyes; then
   AC_MSG_CHECKING(for how to deal with BCPL-style comments)
   case "${host}" in
   *-aix*)
	CFLAGS="$CFLAGS -qcpluscmt"
        AC_MSG_RESULT([-qcpluscmt])
	;;
   *-irix6*)
	CFLAGS="$CFLAGS -Xcpluscomm"
        AC_MSG_RESULT([-Xcpluscomm])
	;;
   *-solaris*)
	CFLAGS="$CFLAGS -xCC"
        AC_MSG_RESULT([-xCC])
	;;
   *)
        AC_MSG_RESULT(nothing needed or no switch known)
	;;
  esac
fi

if test "x$leave_cflags_alone" != xyes; then
	AC_COMPILE_IFELSE(
		[AC_LANG_PROGRAM(
			[[bool flag = true;]],
			[[return flag ? 1 : 0]])],
		[],
		[QF_CC_OPTION_TEST(-std=gnu23,QF_CC_OPTION(-std=gnu23),QF_CC_OPTION(-std=gnu2x))]
	)
fi
AC_MSG_CHECKING([for c23])
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
		[[bool flag = true;]
		 [int bar (void);]],
		[[return bar();]])],
	[AC_MSG_RESULT(yes)],
	[AC_MSG_ERROR(QuakeForge requires C23 to compile)]
)

AS="$CC"
if test "x$SYSTYPE" = "xWIN32"; then
	ASFLAGS="\$(DEFS) \$(CFLAGS) \$(CPPFLAGS) \$(DEFAULT_INCLUDES) \$(INCLUDES) -D_WIN32"
	plugin_ldflags="-no-undefined"
	plugin_libadd="-luser32 -lgdi32 -lwinmm"
	case "$host_os" in
		mingw*)
			if test "x$host" != "x$build"; then
				case "$build_os" in
					cygwin*)
						plugin_libadd=" -L/usr/lib/w32api $plugin_libadd"
						;;
				esac
			fi
			;;
	esac

else
	ASFLAGS="\$(DEFS) \$(CFLAGS) \$(CPPFLAGS) \$(DEFAULT_INCLUDES) \$(INCLUDES)"
	plugin_ldflags=
	plugin_libadd=
fi
ASFLAGS="$ASFLAGS -DQFASM"
CCASFLAGS="$ASFLAGS"
CCAS="$AS"
AC_SUBST(AS)
AC_SUBST(ASFLAGS)
AC_SUBST(CCAS)
AC_SUBST(CCASFLAGS)
QF_SUBST(plugin_ldflags)
QF_SUBST(plugin_libadd)

dnl Finalization of CFLAGS, LDFLAGS, and LIBS
AC_SUBST(CFLAGS)
AC_SUBST(LDFLAGS)
AC_SUBST(LIBS)
