endian=""
FNM_FLAGS=""
case "$host_os" in
	mingw32*)
		mingw=yes
		FNM_FLAGS="-I\$(top_srcdir)/include/win32"
		if test "x$host" != "x$build"; then
			case "$build_os" in
				cygwin*)
					CFLAGS="$CFLAGS -mno-cygwin -mconsole"
					CPPFLAGS="$CPPFLAGS $CFLAGS"
					;;
			esac
		fi
		endian="little"
		;;
	cygwin*)
		cygwin=yes
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
