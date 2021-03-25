dnl Whether to enable XDG support or not
AC_ARG_ENABLE(xdg,
[  --enable-xdg               enable XDG support],
  xdg=$enable_xdg,
  xdg=no
)
if test "x$xdg" != xno; then
  HAVE_XDG=yes
else
  HAVE_XDG=no
fi
AM_CONDITIONAL(HAVE_XDG, test "$HAVE_XDG" = "yes")

dnl Set $prefix and $exec_prefix to $ac_default_prefix if they are not set
test "x$prefix" = "xNONE" && eval prefix="${ac_default_prefix}"
test "x$exec_prefix" = "xNONE" && eval exec_prefix="${prefix}"
test "x$datarootdir" = "x" && eval exec_prefix="${prefix}/share"

if test "x$SYSTYPE" = "xWIN32"; then
	default_globalconf="~/${PACKAGE}.conf"
	default_userconf="~/${PACKAGE}rc"
	default_sharepath="."
	default_userpath="~/${PACKAGE}"
else
	default_globalconf="/etc/${PACKAGE}.conf"
	eval foo="$datarootdir"
	default_sharepath="$foo/games/$PACKAGE"

	if test "x$HAVE_XDG" = "xyes"; then
		default_userconf="~/.config/${PACKAGE}/${PACKAGE}.conf"
		default_userpath="~/.local/share/${PACKAGE}"
  	else
		default_userconf="~/.${PACKAGE}rc"
		default_userpath="~/.$PACKAGE"
  	fi
fi

AC_ARG_WITH(global-cfg,
[  --with-global-cfg=FILE  If set will change the name and location of the
                          global config file used by QuakeForge.  Defaults to
                          /etc/quakeforge.conf.],
globalconf="$withval", globalconf="auto")
if test "x$globalconf" = "xauto" || test "x$globalconf" = "xyes" || \
	test "x$globalconf" = "xno"; then  dnl yes/no sanity checks
	globalconf="$default_globalconf"
fi
AC_DEFINE_UNQUOTED(FS_GLOBALCFG, "$globalconf", [Define this to the location of the global config file])

AC_ARG_WITH(user-cfg,
[  --with-user-cfg=FILE    If set will change the name and location of the
                          user-specific config file used by QuakeForge.
                          Defaults to ~/.quakeforgerc.],
userconf="$withval", userconf="auto")
if test "x$userconf" = "xauto" || test "x$userconf" = "xyes" || \
	test "x$userconf" = "xno"; then  dnl yes/no sanity checks
	userconf="$default_userconf"
fi
AC_DEFINE_UNQUOTED(FS_USERCFG, "$userconf", [Define this to the location of the user config file])

AC_ARG_WITH(sharepath,
[  --with-sharepath=DIR    Use DIR for shared game data, defaults to
                          '.' or \${datarootdir}/games/quakeforge (if new style)],
sharepath=$withval, sharepath="auto")
if test "x$sharepath" = "xauto" -o "x$sharepath" = "xyes" -o "x$sharepath" = "x"; then
	sharepath="$default_sharepath"
elif test "x$sharepath" = xno; then
	sharepath="."
fi
AC_DEFINE_UNQUOTED(FS_SHAREPATH, "$sharepath", [Define this to the shared game directory root])
QF_SUBST(sharepath)

AC_ARG_WITH(userpath,
[  --with-userpath=DIR     Use DIR for unshared game data, defaults to
                          '.' or ~/.quakeforge (if new style)],
userpath=$withval, userpath="auto")
if test "x$userpath" = "xauto" -o "x$userpath" = "xyes" -o "x$userpath" = "x"; then
	userpath="$default_userpath"
elif test "x$userpath" = xno; then
	userpath="."
fi
AC_DEFINE_UNQUOTED(FS_USERPATH, "$userpath", [Define this to the unshared game directory root])

AC_ARG_WITH(plugin-path,
[  --with-plugin-path=DIR  Use DIR for loading plugins, defaults to
                          \${libdir}/quakeforge],
plugindir=$withval, plugindir="auto")

PLUGINDIR="\${libdir}/quakeforge/plugins"
if test "x$plugindir" = "xauto" -o "x$plugindir" = "xyes" -o "x$plugindir" = "x"; then
	plugindir="$PLUGINDIR"
elif test "x$plugindir" = xno; then
	plugindir="."
else
	PLUGINDIR="$plugindir"
fi
eval expanded_plugindir="$plugindir"
eval expanded_plugindir="$expanded_plugindir"
AC_DEFINE_UNQUOTED(FS_PLUGINPATH, "$expanded_plugindir", [Define this to the path from which to load plugins])
AC_SUBST(plugindir)

SHADERDIR="\${libdir}/quakeforge/shaders"
if test "x$shaderdir" = "xauto" -o "x$shaderdir" = "xyes" -o "x$shaderdir" = "x"; then
	shaderdir="$SHADERDIR"
elif test "x$shaderdir" = xno; then
	shaderdir="."
else
	SHADERDIR="$shaderdir"
fi
eval expanded_shaderdir="$shaderdir"
eval expanded_shaderdir="$expanded_shaderdir"
AC_DEFINE_UNQUOTED(FS_SHADERPATH, "$expanded_shaderdir", [Define this to the path from which to load shaders])
AC_SUBST(plugindir)

AC_ARG_WITH(gl-driver,
	[  --with-gl-driver=NAME   Name of OpenGL driver DLL/DSO],
	gl_driver=$withval,
	gl_driver=auto
)
if test "x$gl_driver" = xauto -o "x$gl_driver" = xyes; then
	if test "$SYSTYPE" = WIN32; then
		gl_driver="OPENGL32.DLL"
	elif test "$SYSTYPE" = OPENBSD; then
		gl_driver="libGL.so"
	else
		gl_driver="libGL.so.1"
	fi
fi
AC_DEFINE_UNQUOTED(GL_DRIVER, "$gl_driver", [Define this to the default GL dynamic lib])
