dnl SDL/SDL-GL checks
AC_ARG_ENABLE(sdl, AS_HELP_STRING([--enable-sdl], [enable checking for SDL]))

if test "x$enable_sdl" = xyes; then
	if test "x$PKG_CONFIG" != "x"; then
		PKG_CHECK_MODULES([SDL], [sdl >= 1.2.0], HAVE_SDL=yes, HAVE_SDL=no)
	else
		AM_PATH_SDL(1.2.0, HAVE_SDL=yes, HAVE_SDL=no)
	fi
	if test "x$HAVE_SDL" = "xyes"; then
		case "$host_os" in
			mingw*)
				case "$build_os" in
					cygwin*)
						SDL_LIBS=`echo $SDL_LIBS | sed -e 's/-mwindows//'`
						;;
				esac
			;;
		esac
	fi
fi

dnl SDL-AUDIO checks
AC_ARG_ENABLE(sdl-audio,
	AS_HELP_STRING([  --disable-sdl-audio], [disable checking for SDL-AUDIO]))

if test "x$enable_sdl_audio" != xno; then
	if test "x$HAVE_SDL" = "xyes"; then
		HAVE_SDL_AUDIO=yes
	fi
fi

dnl SDL-CD checks
AC_ARG_ENABLE(sdl-cd,
	AS_HELP_STRING([--disable-sdl-cd], [disable checking for SDL-CD]))
if test "x$enable_sdl_cd" != xno; then
	if test "x$HAVE_SDL" = "xyes"; then
		HAVE_SDL_CD=yes
	fi
fi
