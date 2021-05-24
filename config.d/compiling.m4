if test -d $srcdir/.git; then
	cvs_def_enabled="!= xno"
	cvs_def_disabled="= xyes"
else
	cvs_def_enabled="= xyes"
	cvs_def_disabled="!= xno"
fi

AC_MSG_CHECKING(for CFLAGS pre-set)
leave_cflags_alone=no
if test "x$CFLAGS" != "x"; then
	leave_cflags_alone=yes
	BUILD_TYPE="$BUILD_TYPE Custom"
fi
AC_MSG_RESULT([$leave_cflags_alone])

AC_MSG_CHECKING(for C99 inline)
c99_inline=no
AC_TRY_LINK(
	[inline int foo (int x) { return x * x; }
	 int (*bar) (int) = foo;],
	[],
	c99_inline=no
	AC_MSG_RESULT(no),
	c99_inline=yes
	AC_DEFINE(HAVE_C99INLINE, extern, [define this if using c99 inline])
	AC_MSG_RESULT(yes)
)
AH_VERBATIM([HAVE_C99INLINE],
[#undef HAVE_C99INLINE
#ifdef HAVE_C99INLINE
# define GNU89INLINE
#else
# define GNU89INLINE extern
#endif])

if test "x$GCC" = xyes; then
	set $CC
	shift
	args="$*"
	AC_MSG_CHECKING(for gcc version)
	CCVER="gcc `$CC --version | grep '[[0-9]]\.[[0-9]]' | sed -e 's/.*(GCC)//' -e 's/[[^0-9]]*\([[0-9.]]*\).*/\1/'`"
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
	[  --disable-debug         compile without debugging],
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

AC_ARG_ENABLE(optimize,
	[  --disable-optimize      compile without optimizations (for development)],
	optimize=$enable_optimize,
	optimize=yes
)

QF_CC_OPTION(-Wno-psabi)
dnl QF_CC_OPTION(-msse2)
dnl QF_CC_OPTION(-Wno-psabi)
dnl QF_CC_OPTION(-mavx2)
dnl fma is not used as it is the equivalent of turning on
dnl -funsafe-math-optimizations
dnl QF_CC_OPTION(-mfma)

AC_MSG_CHECKING(for optimization)
if test "x$optimize" = xyes -a "x$leave_cflags_alone" != "xyes"; then
	AC_MSG_RESULT(yes)
	BUILD_TYPE="$BUILD_TYPE Optimize"
	if test "x$GCC" = xyes; then
		saved_cflags="$CFLAGS"
		CFLAGS=""
		QF_CC_OPTION(-frename-registers)
		if test "$CC_MAJ" -ge 4; then
			QF_CC_OPTION(-finline-limit=32000 -Winline)
		fi
		heavy="-O2 $CFLAGS -ffast-math -fno-unsafe-math-optimizations -funroll-loops -fomit-frame-pointer -fexpensive-optimizations"
		CFLAGS="$saved_cflags"
		light="-O2"
		AC_ARG_ENABLE(strict-aliasing,
	[  --enable-strict-aliasing enable the -fstrict-aliasing option of gcc])
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
		[  --with-arch=ARCH        control compiler architecture directly],
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
			AC_TRY_COMPILE(
				[],
				[],
				AC_MSG_RESULT(yes),
				CFLAGS="$save_CFLAGS"
				AC_MSG_RESULT(no)
			)
		fi
		if test $CC_MAJ = 2 -a $CC_MIN = 96; then
			AC_MSG_CHECKING(if align options work)
			save_CFLAGS="$CFLAGS"
			CFLAGS="$CFLAGS -malign-loops=2 -malign-jumps=2 -malign-functions=2"
			AC_TRY_COMPILE(
				[],
				[],
				light="$light -malign-loops=2 -malign-jumps=2 -malign-functions=2"
				AC_MSG_RESULT(yes),
				AC_MSG_RESULT(no)
			)
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
	[  --enable-profile        compile with profiling (for development)],
	profile=$enable_profile
)
if test "x$profile" = xyes; then
	BUILD_TYPE="$BUILD_TYPE Profile"
	if test "x$GCC" = xyes; then
		CFLAGS="`echo $CFLAGS | sed -e 's/-fomit-frame-pointer//g'` -pg"
		LDFLAGS="$LDFLAGS -pg"
	else
		CFLAGS="$CFLAGS -p"
	fi
fi

check_pipe=no
if test "x$GCC" = xyes; then
	dnl Check for -pipe vs -save-temps.
	AC_MSG_CHECKING(for -pipe vs -save-temps)
	AC_ARG_ENABLE(save-temps,
		[  --enable-save-temps     save temporary files],
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
[  --enable-coverage       Enable generation of data for gcov])
if test "x$enable_coverage" = xyes; then
	QF_CC_OPTION(-fprofile-arcs -ftest-coverage)
fi

dnl QuakeForge uses lots of BCPL-style (//) comments, which can cause problems
dnl with many compilers that do not support the latest ISO standards. Well,
dnl that is our cover story -- the reality is that we like them and do not want
dnl to give them up. :)
dnl Make the compiler swallow its pride...
if test "x$GCC" != xyes; then
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

AC_ARG_ENABLE(Werror,
[  --disable-Werror        Do not treat warnings as errors])
dnl We want warnings, lots of warnings...
dnl The help text should be INVERTED before release!
dnl when in git, this test defaults to ENABLED.
dnl In a release, this test defaults to DISABLED.
if test "x$GCC" = "xyes"; then
	if test "x$enable_Werror" $cvs_def_enabled; then
		CFLAGS="$CFLAGS -Wall -Werror -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations"
	else
		CFLAGS="$CFLAGS -Wall"
	fi
	CFLAGS="$CFLAGS -fno-common"
fi

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
