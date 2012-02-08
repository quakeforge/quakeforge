dnl ==================================================================
dnl Find out what to build
dnl ==================================================================

QF_WITH_TARGETS(
	clients,
	[  --with-clients=<list>   compile clients in <list>:],
	[3dfx,fbdev,glx,glslx,mgl,sdl,sdl32,sgl,sglsl,svga,wgl,x11],dummy
)
QF_WITH_TARGETS(
	servers,
	[  --with-servers=<list>   compile dedicated server:],
	[master,nq,qw,qtv],dummy
)
QF_WITH_TARGETS(
	tools,
	[  --with-tools=<list>     compile qf tools:],
	[bsp2img,carne,gsc,pak,qfbsp,qfcc,qflight,qflmp,qfmodelgen,qfvis,qwaq,wad,wav],dummy
)

unset CL_TARGETS
HW_TARGETS=""
QTV_TARGETS=""
QW_TARGETS=""
NQ_TARGETS=""
QW_DESKTOP_DATA=""
NQ_DESKTOP_DATA=""

BUILD_GL=no
BUILD_GLSL=no
BUILD_SW32=no
BUILD_SW=no
CAN_BUILD_GL=no
CAN_BUILD_GLSL=no
CAN_BUILD_SW32=no
CAN_BUILD_SW=no

CD_TARGETS=""
SND_PLUGIN_TARGETS="snd_output_disk.la"
SND_REND_TARGETS=""
SND_TARGETS=""
VID_MODEL_TARGETS=""
VID_REND_TARGETS=""
VID_REND_NOINST_TARGETS=""
VID_TARGETS=""

if test "x$HAVE_FBDEV" = xyes; then
	if test "x$ENABLE_clients_fbdev" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-fbdev\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-fbdev\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS FBDEV"
		VID_TARGETS="$VID_TARGETS libQFfbdev.la"
		BUILD_SW=yes
		QF_NEED(vid, [common sw])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
	CAN_BUILD_SW=yes
fi
if test "x$HAVE_X" = xyes; then
	CAN_BUILD_GL=yes
	CAN_BUILD_GLSL=yes
	CAN_BUILD_SW=yes
	if test "x$ENABLE_clients_glx" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-glx\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-glx\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA quakeforge-qw-glx.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA quakeforge-nq-glx.desktop"
		CL_TARGETS="$CL_TARGETS GLX"
		VID_TARGETS="$VID_TARGETS libQFglx.la"
		BUILD_GL=yes
		QF_NEED(vid, [common gl x11])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
	if test "x$ENABLE_clients_glslx" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-glslx\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-glslx\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA quakeforge-qw-glx.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA quakeforge-nq-glslx.desktop"
		CL_TARGETS="$CL_TARGETS GLSLX"
		VID_TARGETS="$VID_TARGETS libQFglslx.la"
		BUILD_GLSL=yes
		QF_NEED(vid, [common glsl x11])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
	if test "x$ENABLE_clients_x11" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-x11\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-x11\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA quakeforge-qw-x11.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA quakeforge-nq-x11.desktop"
		CL_TARGETS="$CL_TARGETS X11"
		VID_TARGETS="$VID_TARGETS libQFx11.la"
		BUILD_SW=yes
		QF_NEED(vid, [common sw x11])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
fi
if test "x$HAVE_MGL" = xyes; then
	if test "x$ENABLE_clients_mgl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-mgl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-mgl\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS MGL"
		VID_TARGETS="$VID_TARGETS libQFwgl.la"
		BUILD_SW=yes
		QF_NEED(vid, [common sw])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
	CAN_BUILD_SW=yes
fi
if test "x$HAVE_SDL" = xyes; then
	if test "x$ENABLE_clients_sdl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sdl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sdl\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA quakeforge-qw-sdl.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA quakeforge-nq-sdl.desktop"
		CL_TARGETS="$CL_TARGETS SDL"
		VID_TARGETS="$VID_TARGETS libQFsdl.la"
		BUILD_SW=yes
		QF_NEED(vid, [common sdl sw])
		QF_NEED(qw, [client common sdl])
		QF_NEED(nq, [client common sdl])
		QF_NEED(console, [client])
	fi
	CAN_BUILD_SW=yes
	if test "x$ENABLE_clients_sdl32" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sdl32\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sdl32\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA quakeforge-qw-sdl32.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA quakeforge-nq-sdl32.desktop"
		CL_TARGETS="$CL_TARGETS SDL32"
		VID_TARGETS="$VID_TARGETS libQFsdl32.la"
		BUILD_SW32=yes
		QF_NEED(vid, [common sdl sw32])
		QF_NEED(qw, [client common sdl])
		QF_NEED(nq, [client common sdl])
		QF_NEED(console, [client])
	fi
	CAN_BUILD_SW32=yes
	if test "x$ENABLE_clients_sgl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sgl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sgl\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA quakeforge-qw-sgl.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA quakeforge-nq-sgl.desktop"
		CL_TARGETS="$CL_TARGETS SDL-GL"
		VID_TARGETS="$VID_TARGETS libQFsgl.la"
		BUILD_GL=yes
		CAN_BUILD_GL=yes
		QF_NEED(vid, [common sdl gl])
		QF_NEED(qw, [client common sdl])
		QF_NEED(nq, [client common sdl])
		QF_NEED(console, [client])
	fi
	if test "x$ENABLE_clients_sglsl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sglsl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sglsl\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA quakeforge-qw-sglsl.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA quakeforge-nq-sglsl.desktop"
		CL_TARGETS="$CL_TARGETS SDL-GLSL"
		VID_TARGETS="$VID_TARGETS libQFsglsl.la"
		BUILD_GLSL=yes
		CAN_BUILD_GLSL=yes
		QF_NEED(vid, [common sdl glsl])
		QF_NEED(qw, [client common sdl])
		QF_NEED(nq, [client common sdl])
		QF_NEED(console, [client])
	fi
fi
if test "x$HAVE_SVGA" = xyes; then
	if test "x$ENABLE_clients_svga" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-svga\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-svga\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS SVGAlib"
		VID_TARGETS="$VID_TARGETS libQFsvga.la"
		BUILD_SW=yes
		QF_NEED(vid, [asm common svga sw])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
	CAN_BUILD_SW=yes
	if test "x$ENABLE_clients_3dfx" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-3dfx\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-3dfx\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS 3dfx"
		VID_TARGETS="$VID_TARGETS libQFtdfx.la"
		BUILD_GL=yes
		QF_NEED(vid, [asm common gl svga])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
	CAN_BUILD_GL=yes
fi
if test "x$mingw" = xyes; then
	if test "x$ENABLE_clients_wgl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-wgl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-wgl\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS WGL"
		VID_TARGETS="$VID_TARGETS libQFwgl.la"
		BUILD_GL=yes
		QF_NEED(vid, [common gl])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
	CAN_BUILD_GL=yes
fi

unset SV_TARGETS
if test "x$ENABLE_servers_nq" = xyes; then
	NQ_TARGETS="nq-server\$(EXEEXT) $NQ_TARGETS"
	SV_TARGETS="$SV_TARGETS nq"
	QF_NEED(nq, [common server])
	QF_NEED(console, [server])
fi
if test "x$ENABLE_servers_qtv" = xyes; then
	QTV_TARGETS="qtv\$(EXEEXT) $QTV_TARGETS"
	SV_TARGETS="$SV_TARGETS qtv"
#	QF_NEED(qtv, [common server])
	QF_NEED(console, [server])
fi
if test "x$ENABLE_servers_master" = xyes; then
	HW_TARGETS="hw-master\$(EXEEXT) $HW_TARGETS"
	QW_TARGETS="qw-master\$(EXEEXT) $QW_TARGETS"
	SV_TARGETS="$SV_TARGETS master"
fi
if test "x$ENABLE_servers_qw" = xyes; then
	QW_TARGETS="qw-server\$(EXEEXT) $QW_TARGETS"
	SV_TARGETS="$SV_TARGETS qw"
	QF_NEED(qw, [common server])
	QF_NEED(console, [server])
fi

unset TOOLS_TARGETS
if test "x$ENABLE_tools_bsp2img" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS bsp2img"
fi
if test "x$ENABLE_tools_carne" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS carne"
fi
if test "x$ENABLE_tools_gsc" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS gsc"
fi
if test "x$ENABLE_tools_pak" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS pak"
fi
if test "x$ENABLE_tools_qfbsp" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qfbsp"
fi
if test "x$ENABLE_tools_qfcc" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qfcc"
fi
if test "x$ENABLE_tools_qflight" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qflight"
fi
if test "x$ENABLE_tools_qflmp" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qflmp"
fi
if test "x$ENABLE_tools_qfmodelgen" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qfmodelgen"
fi
if test "x$ENABLE_tools_qfvis" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qfvis"
fi
if test "x$ENABLE_tools_qwaq" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS qwaq"
fi
if test "x$ENABLE_tools_wad" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS wad"
fi
if test "x$ENABLE_tools_wav" = xyes; then
	TOOLS_TARGETS="$TOOLS_TARGETS wav"
fi

AM_CONDITIONAL(BUILD_BSP2IMG, test "$ENABLE_tools_bsp2img" = "yes")
AM_CONDITIONAL(BUILD_CARNE, test "$ENABLE_tools_carne" = "yes")
AM_CONDITIONAL(BUILD_PAK, test "$ENABLE_tools_pak" = "yes")
AM_CONDITIONAL(BUILD_QFBSP, test "$ENABLE_tools_qfbsp" = "yes")
AM_CONDITIONAL(BUILD_QFCC, test "$ENABLE_tools_qfcc" = "yes")
AM_CONDITIONAL(BUILD_QFLIGHT, test "$ENABLE_tools_qflight" = "yes")
AM_CONDITIONAL(BUILD_QFLMP, test "$ENABLE_tools_qflmp" = "yes")
AM_CONDITIONAL(BUILD_QFMODELGEN, test "$ENABLE_tools_qfmodelgen" = "yes")
AM_CONDITIONAL(BUILD_QFVIS, test "$ENABLE_tools_qfvis" = "yes")
AM_CONDITIONAL(BUILD_QWAQ, test "$ENABLE_tools_qwaq" = "yes" -a "$ENABLE_tools_qfcc" = "yes")
AM_CONDITIONAL(BUILD_WAD, test "$ENABLE_tools_wad" = "yes")
AM_CONDITIONAL(BUILD_WAV, test "$ENABLE_tools_wav" = "yes")

AM_CONDITIONAL(BUILD_RUAMOKO, test "$ENABLE_tools_qfcc" = "yes" -a "$ENABLE_tools_pak" = "yes")

if test "x$BUILD_SW" = xyes; then
	VID_REND_NOINST_TARGETS="$VID_REND_NOINST_TARGETS libQFrenderer_sw.la"
	VID_MODEL_TARGETS="$VID_MODEL_TARGETS libQFmodels_sw.la"
fi
if test "x$BUILD_SW32" = xyes; then
	VID_REND_TARGETS="$VID_REND_TARGETS libQFrenderer_sw32.la"
	if test "x$BUILD_SW" != xyes; then
		VID_MODEL_TARGETS="$VID_MODEL_TARGETS libQFmodels_sw.la"
	fi
fi
if test "x$BUILD_GL" = xyes; then
	VID_REND_TARGETS="$VID_REND_TARGETS libQFrenderer_gl.la"
	VID_MODEL_TARGETS="$VID_MODEL_TARGETS libQFmodels_gl.la"
fi
if test "x$BUILD_GLSL" = xyes; then
	VID_REND_TARGETS="$VID_REND_TARGETS libQFrenderer_glsl.la"
	VID_MODEL_TARGETS="$VID_MODEL_TARGETS libQFmodels_glsl.la"
fi

QF_PROCESS_NEED(vid, [asm common gl glsl sdl sw sw32 svga x11])
QF_PROCESS_NEED(qw, [client common sdl server], a)
QF_PROCESS_NEED(nq, [client common sdl server], a)

AC_SUBST(CAN_BUILD_GL)
AC_SUBST(CAN_BUILD_GLSL)
AC_SUBST(CAN_BUILD_SW)
AC_SUBST(CAN_BUILD_SW32)

AC_SUBST(HAVE_FBDEV)
AC_SUBST(HAVE_SDL)
AC_SUBST(HAVE_SVGA)

SND_OUTPUT_DEFAULT=""
if test -n "$CL_TARGETS"; then
	CD_TARGETS="libQFcd.la"
	SND_TARGETS="libQFsound.la"
	AUDIO_TARGETS="testsound\$(EXEEXT)"
	JOY_TARGETS="libQFjs.la"

	if test "$SOUND_TYPES"; then
		SND_REND_TARGETS="$SND_REND_TARGETS snd_render_default.la"
		if test "`echo $SOUND_TYPES | grep JACK`"; then
			SND_REND_TARGETS="$SND_REND_TARGETS snd_render_jack.la"
		fi
	fi

	# priority sorted list for default sound driver in order of increasing
	# priority. default is no driver.
	# NOTE both lists must match: SNDTYPE_LIST is the name as normally printed
	# SNDDRIVER_LIST is the name as used by the plugin
	SNDTYPE_LIST="SDL MME SGI SUN Win32 DirectX OSS ALSA"
	SNDDRIVER_LIST="sdl mme sgi sun win dx oss alsa"
	set $SNDDRIVER_LIST
	for t in $SNDTYPE_LIST; do
		if test "`echo $SOUND_TYPES | grep $t`"; then
			SND_PLUGIN_TARGETS="$SND_PLUGIN_TARGETS snd_output_$1.la"
			SND_OUTPUT_DEFAULT="$1"
		fi
		shift
	done
else
	unset CDTYPE
	CD_PLUGIN_TARGETS=""
	CD_TARGETS=""
	JOY_TARGETS=""
	SND_PLUGIN_TARGETS=""
	SND_REND_TARGETS=""
	SND_TARGETS=""
	AUDIO_TARGETS=""
	unset SOUND_TYPES
fi
AC_DEFINE_UNQUOTED(SND_OUTPUT_DEFAULT, "$SND_OUTPUT_DEFAULT", [Define this to the default sound output driver.])

SERVER_PLUGIN_TARGETS=""
if test x$console_need_server = xyes; then
	SERVER_PLUGIN_TARGETS="console_server.la"
fi
SERVER_PLUGIN_STATIC=""
CLIENT_PLUGIN_TARGETS=""
if test x$console_need_client = xyes; then
	CLIENT_PLUGIN_TARGETS="console_client.la"
fi
CLIENT_PLUGIN_STATIC=""
CD_PLUGIN_STATIC=""
SND_PLUGIN_STATIC=""
SND_REND_STATIC=""

if test "x$enable_shared" = xno; then
	PREFER_PIC=
	PREFER_NON_PIC=
else
	PREFER_PIC="-prefer-pic ${VISIBILITY}"
	PREFER_NON_PIC="-prefer-non-pic ${VISIBILITY}"
fi
if test "x$enable_static" = xno; then
	STATIC=
else
	STATIC=-static
fi
AC_SUBST(PREFER_PIC)
AC_SUBST(PREFER_NON_PIC)
AC_SUBST(STATIC)

AC_ARG_WITH(static-plugins,
[  --with-static-plugins   build plugins into executable rather than separate],
	static_plugins="$withval", static_plugins=auto)
if test "x$static_plugins" = xauto; then
	if test "x$enable_shared" = xno -o "x$SYSTYPE" = xWIN32; then
		static_plugins=yes
	fi
fi
if test "x$static_plugins" = xyes; then
	AC_DEFINE(STATIC_PLUGINS, 1, [Define this if you are building static plugins])
	SERVER_PLUGIN_STATIC="$SERVER_PLUGIN_TARGETS"
	SERVER_PLUGIN_TARGETS=""
	CLIENT_PLUGIN_STATIC="$CLIENT_PLUGIN_TARGETS"
	CLIENT_PLUGIN_TARGETS=""
	CD_PLUGIN_STATIC="$CD_PLUGIN_TARGETS"
	CD_PLUGIN_TARGETS=""
	SND_PLUGIN_STATIC="$SND_PLUGIN_TARGETS"
	SND_PLUGIN_TARGETS=""
	SND_REND_STATIC="$SND_REND_TARGETS"
	SND_REND_TARGETS=""
	if test -n "$SOUND_TYPES"; then
		SOUND_TYPES="$SOUND_TYPES (static)"
		CDTYPE="$CDTYPE (static)"
	fi
fi

dnl Do not use -module here, it belongs in makefile.am due to automake
dnl needing it there to work correctly
SERVER_PLUGIN_STATIC_LIBS=""
CLIENT_PLUGIN_STATIC_LIBS=""
CD_PLUGIN_STATIC_LIBS=""
SND_PLUGIN_STATIC_LIBS=""
SND_REND_STATIC_LIBS=""
SERVER_PLUGIN_LIST="{0, 0}"
CLIENT_PLUGIN_LIST="{0, 0}"
CD_PLUGIN_LIST="{0, 0}"
SND_OUTPUT_LIST="{0, 0}"
SND_RENDER_LIST="{0, 0}"
SERVER_PLUGIN_PROTOS=""
CLIENT_PLUGIN_PROTOS=""
CD_PLUGIN_PROTOS=""
SND_OUTPUT_PROTOS=""
SND_RENDER_PROTOS=""
for l in $SERVER_PLUGIN_STATIC; do
	SERVER_PLUGIN_STATIC_LIBS="$SERVER_PLUGIN_STATIC_LIBS "'$(top_builddir)'"/libs/console/$l"
	n="`echo $l | sed -e 's/\(.*\)\.la/\1/'`"
	SERVER_PLUGIN_LIST='{"'"$n"'"'", ${n}_PluginInfo},$SERVER_PLUGIN_LIST"
	SERVER_PLUGIN_PROTOS="$SERVER_PLUGIN_PROTOS extern plugin_t *${n}_PluginInfo (void);"
done
for l in $CLIENT_PLUGIN_STATIC; do
	CLIENT_PLUGIN_STATIC_LIBS="$CLIENT_PLUGIN_STATIC_LIBS "'$(top_builddir)'"/libs/console/$l"
	n="`echo $l | sed -e 's/\(.*\)\.la/\1/'`"
	CLIENT_PLUGIN_LIST='{"'"$n"'"'", ${n}_PluginInfo},$CLIENT_PLUGIN_LIST"
	CLIENT_PLUGIN_PROTOS="$CLIENT_PLUGIN_PROTOS extern plugin_t *${n}_PluginInfo (void);"
done
for l in $CD_PLUGIN_STATIC; do
	CD_PLUGIN_STATIC_LIBS="$CD_PLUGIN_STATIC_LIBS $l"
	n="`echo $l | sed -e 's/\(.*\)\.la/\1/'`"
	CD_PLUGIN_LIST='{"'"$n"'"'", ${n}_PluginInfo},$CD_PLUGIN_LIST"
	CD_PLUGIN_PROTOS="$CD_PLUGIN_PROTOS extern plugin_t *${n}_PluginInfo (void);"
done
for l in $SND_PLUGIN_STATIC; do
	SND_PLUGIN_STATIC_LIBS="$SND_PLUGIN_STATIC_LIBS targets/$l"
	n="`echo $l | sed -e 's/\(.*\)\.la/\1/'`"
	SND_OUTPUT_LIST='{"'"$n"'"'", ${n}_PluginInfo},$SND_OUTPUT_LIST"
	SND_OUTPUT_PROTOS="$SND_OUTPUT_PROTOS extern plugin_t *${n}_PluginInfo (void);"
done
for l in $SND_REND_STATIC; do
	SND_REND_STATIC_LIBS="$SND_REND_STATIC_LIBS renderer/$l"
	n="`echo $l | sed -e 's/\(.*\)\.la/\1/'`"
	SND_RENDER_LIST='{"'"$n"'"'", ${n}_PluginInfo},$SND_RENDER_LIST"
	SND_RENDER_PROTOS="$SND_RENDER_PROTOS extern plugin_t *${n}_PluginInfo (void);"
done
AC_DEFINE_UNQUOTED(SERVER_PLUGIN_LIST, $SERVER_PLUGIN_LIST, [list of server plugins])
AC_DEFINE_UNQUOTED(SERVER_PLUGIN_PROTOS, $SERVER_PLUGIN_PROTOS, [list of server prototypes])
AC_DEFINE_UNQUOTED(CLIENT_PLUGIN_LIST, $CLIENT_PLUGIN_LIST, [list of client plugins])
AC_DEFINE_UNQUOTED(CLIENT_PLUGIN_PROTOS, $CLIENT_PLUGIN_PROTOS, [list of client prototypes])
AC_DEFINE_UNQUOTED(CD_PLUGIN_LIST, $CD_PLUGIN_LIST, [list of cd plugins])
AC_DEFINE_UNQUOTED(CD_PLUGIN_PROTOS, $CD_PLUGIN_PROTOS, [list of cd prototypes])
AC_DEFINE_UNQUOTED(SND_OUTPUT_LIST, $SND_OUTPUT_LIST, [list of sound output plugins])
AC_DEFINE_UNQUOTED(SND_OUTPUT_PROTOS, $SND_OUTPUT_PROTOS, [list of sound output prototypes])
AC_DEFINE_UNQUOTED(SND_RENDER_LIST, $SND_RENDER_LIST, [list of sound render plugins])
AC_DEFINE_UNQUOTED(SND_RENDER_PROTOS, $SND_RENDER_PROTOS, [list of sound render prototypes])

AC_SUBST(HW_TARGETS)
AC_SUBST(NQ_TARGETS)
AC_SUBST(NQ_DESKTOP_DATA)
AC_SUBST(QTV_TARGETS)
AC_SUBST(QW_TARGETS)
AC_SUBST(QW_DESKTOP_DATA)
AC_SUBST(SERVER_PLUGIN_STATIC)
AC_SUBST(SERVER_PLUGIN_STATIC_LIBS)
AC_SUBST(SERVER_PLUGIN_TARGETS)
AC_SUBST(CLIENT_PLUGIN_STATIC)
AC_SUBST(CLIENT_PLUGIN_STATIC_LIBS)
AC_SUBST(CLIENT_PLUGIN_TARGETS)
AC_SUBST(CD_PLUGIN_STATIC)
AC_SUBST(CD_PLUGIN_STATIC_LIBS)
AC_SUBST(CD_PLUGIN_TARGETS)
AC_SUBST(CD_TARGETS)
AC_SUBST(JOY_TARGETS)
AC_SUBST(SND_PLUGIN_STATIC)
AC_SUBST(SND_PLUGIN_STATIC_LIBS)
AC_SUBST(SND_PLUGIN_TARGETS)
AC_SUBST(SND_REND_STATIC)
AC_SUBST(SND_REND_STATIC_LIBS)
AC_SUBST(SND_REND_TARGETS)
AC_SUBST(SND_TARGETS)
AC_SUBST(AUDIO_TARGETS)
AC_SUBST(VID_MODEL_TARGETS)
AC_SUBST(VID_REND_TARGETS)
AC_SUBST(VID_REND_NOINST_TARGETS)
AC_SUBST(VID_TARGETS)

QF_DEPS(BSP2IMG,
	[],
	[$(top_builddir)/libs/image/libQFimage.la $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFBSP,
	[-I$(top_srcdir)/tools/qfbsp/include],
	[$(top_builddir)/libs/models/libQFmodels.la $(top_builddir)/libs/image/libQFimage.la $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFCC,
	[-I$(top_srcdir)/tools/qfcc/include],
	[$(top_builddir)/libs/gamecode/libQFgamecode.la $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFLIGHT,
	[-I$(top_srcdir)/tools/qflight/include],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFLMP,
	[],
	[$(top_builddir)/libs/image/libQFimage.la $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFMODELGEN,
	[-I$(top_srcdir)/tools/qfmodelgen/include],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFVIS,
	[-I$(top_srcdir)/tools/qfvis/include],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QWAQ,
	[],
	[$(top_builddir)/libs/ruamoko/libQFruamoko.la $(top_builddir)/libs/gamecode/libQFgamecode.la $(top_builddir)/libs/gib/libQFgib.la $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(CARNE,
	[],
	[$(top_builddir)/libs/gib/libQFgib.la $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(PAK,
	[],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(WAD,
	[],
	[$(top_builddir)/libs/image/libQFimage.la $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(WAV,
	[],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)

AM_CONDITIONAL(BUILD_GL, test "$BUILD_GL" = "yes")
AM_CONDITIONAL(BUILD_GLSL, test "$BUILD_GLSL" = "yes")
AM_CONDITIONAL(BUILD_SW, test "$BUILD_SW" = "yes")
AM_CONDITIONAL(BUILD_SW_ASM, test "$BUILD_SW" = "yes" -a "$ASM_ARCH" = "yes")
AM_CONDITIONAL(BUILD_SW_MOD, test "$BUILD_SW" = "yes" -o "$BUILD_SW32" = "yes")
AM_CONDITIONAL(BUILD_SW32, test "$BUILD_SW32" = "yes")
