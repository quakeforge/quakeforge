endian=""
FNM_FLAGS=""

case "$host_os" in
	mingw32*)
		mingw=yes
		FNM_FLAGS="-I\$(top_srcdir)/include/win32"
		if test "x$host" != "x$build"; then
			case "$build_os" in
				cygwin*)
					CFLAGS="$CFLAGS -mconsole -D__USE_MINGW_ANSI_STDIO"
					CPPFLAGS="$CPPFLAGS $CFLAGS"
					;;
			esac
		fi
		SYSTYPE=WIN32
		AC_DEFINE(NEED_GNUPRINTF)
		endian="little"
		;;
	cygwin*)
		cygwin=yes
		SYSTYPE=WIN32
		AC_DEFINE(NEED_GNUPRINTF)
		FNM_FLAGS="-I\$(top_srcdir)/include/win32"
		if test "x$host" != "x$build"; then
			CC="$host_cpu-$host_os-gcc"
			AS="$CC"
			AR="$host_cpu_$host_os-ar"
			RANLIB="$host_cpu-$host_os-ranlib"
			endian="little"
		fi
	;;
esac
AC_SUBST(FNM_FLAGS)

AH_VERBATIM([NEED_GNUPRINTF],
[/* Define this if gnu_prinf is needed instead of printf for format attributes*/
#undef NEED_GNUPRINTF
#ifdef NEED_GNUPRINTF
# define PRINTF gnu_printf
#else
# define PRINTF printf
#endif])
