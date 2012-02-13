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

CD_TARGETS=""
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
		QF_NEED(QFrenderer, [sw])
		QF_NEED(QFmodels, [sw])
		QF_NEED(alias, [sw])
		QF_NEED(brush, [sw])
		QF_NEED(sprite, [sw])
		if test "x$ASM_ARCH" = "xyes"; then
			QF_NEED(swrend, [asm])
		fi
		QF_NEED(vid, [common sw])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
fi
if test "x$HAVE_X" = xyes; then
	if test "x$ENABLE_clients_glx" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-glx\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-glx\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA quakeforge-qw-glx.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA quakeforge-nq-glx.desktop"
		CL_TARGETS="$CL_TARGETS GLX"
		VID_TARGETS="$VID_TARGETS libQFglx.la"
		QF_NEED(QFrenderer, [gl])
		QF_NEED(QFmodels, [gl])
		QF_NEED(alias, [gl])
		QF_NEED(brush, [gl])
		QF_NEED(sprite, [gl])
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
		QF_NEED(QFrenderer, [glsl])
		QF_NEED(QFmodels, [glsl])
		QF_NEED(alias, [glsl])
		QF_NEED(brush, [glsl])
		QF_NEED(sprite, [glsl])
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
		QF_NEED(QFrenderer, [sw])
		QF_NEED(QFmodels, [sw])
		QF_NEED(alias, [sw])
		QF_NEED(brush, [sw])
		QF_NEED(sprite, [sw])
		if test "x$ASM_ARCH" = "xyes"; then
			QF_NEED(swrend, [asm])
		fi
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
		QF_NEED(QFrenderer, [sw])
		QF_NEED(QFmodels, [sw])
		QF_NEED(alias, [sw])
		QF_NEED(brush, [sw])
		QF_NEED(sprite, [sw])
		if test "x$ASM_ARCH" = "xyes"; then
			QF_NEED(swrend, [asm])
		fi
		QF_NEED(vid, [common sw])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
fi
if test "x$HAVE_SDL" = xyes; then
	if test "x$ENABLE_clients_sdl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sdl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sdl\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA quakeforge-qw-sdl.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA quakeforge-nq-sdl.desktop"
		CL_TARGETS="$CL_TARGETS SDL"
		VID_TARGETS="$VID_TARGETS libQFsdl.la"
		QF_NEED(QFrenderer, [sw])
		QF_NEED(QFmodels, [sw])
		QF_NEED(alias, [sw])
		QF_NEED(brush, [sw])
		QF_NEED(sprite, [sw])
		if test "x$ASM_ARCH" = "xyes"; then
			QF_NEED(swrend, [asm])
		fi
		QF_NEED(vid, [common sdl sw])
		QF_NEED(qw, [client common sdl])
		QF_NEED(nq, [client common sdl])
		QF_NEED(console, [client])
	fi
	if test "x$ENABLE_clients_sdl32" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sdl32\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sdl32\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA quakeforge-qw-sdl32.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA quakeforge-nq-sdl32.desktop"
		CL_TARGETS="$CL_TARGETS SDL32"
		VID_TARGETS="$VID_TARGETS libQFsdl32.la"
		QF_NEED(QFrenderer, [sw32])
		QF_NEED(QFmodels, [sw])
		QF_NEED(alias, [sw])
		QF_NEED(brush, [sw])
		QF_NEED(sprite, [sw])
		QF_NEED(vid, [common sdl sw32])
		QF_NEED(qw, [client common sdl])
		QF_NEED(nq, [client common sdl])
		QF_NEED(console, [client])
	fi
	if test "x$ENABLE_clients_sgl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sgl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sgl\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA quakeforge-qw-sgl.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA quakeforge-nq-sgl.desktop"
		CL_TARGETS="$CL_TARGETS SDL-GL"
		VID_TARGETS="$VID_TARGETS libQFsgl.la"
		QF_NEED(QFrenderer, [gl])
		QF_NEED(QFmodels, [gl])
		QF_NEED(alias, [gl])
		QF_NEED(brush, [gl])
		QF_NEED(sprite, [gl])
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
		QF_NEED(QFrenderer, [glsl])
		QF_NEED(QFmodels, [glsl])
		QF_NEED(alias, [glsl])
		QF_NEED(brush, [glsl])
		QF_NEED(sprite, [glsl])
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
		QF_NEED(QFrenderer, [sw])
		QF_NEED(QFmodels, [sw])
		QF_NEED(alias, [sw])
		QF_NEED(brush, [sw])
		QF_NEED(sprite, [sw])
		if test "x$ASM_ARCH" = "xyes"; then
			QF_NEED(swrend, [asm])
		fi
		QF_NEED(vid, [asm common svga sw])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
	if test "x$ENABLE_clients_3dfx" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-3dfx\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-3dfx\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS 3dfx"
		VID_TARGETS="$VID_TARGETS libQFtdfx.la"
		QF_NEED(QFrenderer, [gl])
		QF_NEED(QFmodels, [gl])
		QF_NEED(alias, [gl])
		QF_NEED(brush, [gl])
		QF_NEED(sprite, [gl])
		QF_NEED(vid, [asm common gl svga])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
fi
if test "x$mingw" = xyes; then
	if test "x$ENABLE_clients_wgl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-wgl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-wgl\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS WGL"
		VID_TARGETS="$VID_TARGETS libQFwgl.la"
		QF_NEED(QFrenderer, [gl])
		QF_NEED(QFmodels, [gl])
		QF_NEED(alias, [gl])
		QF_NEED(brush, [gl])
		QF_NEED(sprite, [gl])
		QF_NEED(vid, [common gl])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
	fi
fi

unset SV_TARGETS
if test "x$ENABLE_servers_nq" = xyes; then
	NQ_TARGETS="nq-server\$(EXEEXT) $NQ_TARGETS"
	SV_TARGETS="$SV_TARGETS nq"
	QF_NEED(nq, [common server])
	QF_NEED(console, [server])
	QF_NEED(top, [nq])
fi
if test "x$ENABLE_servers_qtv" = xyes; then
	QTV_TARGETS="qtv\$(EXEEXT) $QTV_TARGETS"
	SV_TARGETS="$SV_TARGETS qtv"
#	QF_NEED(qtv, [common server])
	QF_NEED(console, [server])
	QF_NEED(top, [qtv])
fi
if test "x$ENABLE_servers_master" = xyes; then
	HW_TARGETS="hw-master\$(EXEEXT) $HW_TARGETS"
	QW_TARGETS="qw-master\$(EXEEXT) $QW_TARGETS"
	SV_TARGETS="$SV_TARGETS master"
	QF_NEED(top, [hw qw])
fi
if test "x$ENABLE_servers_qw" = xyes; then
	QW_TARGETS="qw-server\$(EXEEXT) $QW_TARGETS"
	SV_TARGETS="$SV_TARGETS qw"
	QF_NEED(qw, [common server])
	QF_NEED(console, [server])
	QF_NEED(top, [qw])
fi

if test "x$ENABLE_tools_bsp2img" = xyes; then
	QF_NEED(tools,[bsp2img])
fi
if test "x$ENABLE_tools_carne" = xyes; then
	QF_NEED(tools,[carne])
fi
if test "x$ENABLE_tools_pak" = xyes; then
	QF_NEED(tools,[pak])
fi
if test "x$ENABLE_tools_qfbsp" = xyes; then
	QF_NEED(tools,[qfbsp])
fi
if test "x$ENABLE_tools_qfcc" = xyes; then
	QF_NEED(tools,[qfcc])
fi
if test "x$ENABLE_tools_qflight" = xyes; then
	QF_NEED(tools,[qflight])
fi
if test "x$ENABLE_tools_qflmp" = xyes; then
	QF_NEED(tools,[qflmp])
fi
if test "x$ENABLE_tools_qfmodelgen" = xyes; then
	QF_NEED(tools,[qfmodelgen])
fi
if test "x$ENABLE_tools_qfvis" = xyes; then
	QF_NEED(tools,[qfvis])
fi
if test "x$ENABLE_tools_qwaq" = xyes; then
	QF_NEED(tools,[qwaq])
fi
if test "x$ENABLE_tools_wad" = xyes; then
	QF_NEED(tools,[wad])
fi
if test "x$ENABLE_tools_wav" = xyes; then
	QF_NEED(tools,[wav])
fi

QF_NEED(top, [libs hw nq qtv qw])

QF_PROCESS_NEED_DIRS(tools,[bsp2img carne pak qfbsp qfcc qflight qflmp qfmodelgen qfvis qwaq wad wav])
QF_PROCESS_NEED_FUNC(tools,[bsp2img carne pak qfbsp qfcc qflight qflmp qfmodelgen qfvis qwaq wad wav], QF_NEED(top,tools))

if test "$ENABLE_tools_qfcc" = "yes" -a "$ENABLE_tools_pak" = "yes"; then
	QF_NEED(top, [ruamoko])
fi

QF_PROCESS_NEED_DIRS(top, [libs hw nq qtv qw tools ruamoko])

QF_PROCESS_NEED_LIBS(swrend, [asm])
QF_PROCESS_NEED_LIBS(QFrenderer, [gl glsl sw sw32])
QF_PROCESS_NEED_DIRS(QFrenderer, [gl glsl sw sw32])
QF_PROCESS_NEED_LIBS(QFmodels, [gl glsl sw])
QF_PROCESS_NEED_LIBS(alias, [gl glsl sw])
QF_PROCESS_NEED_LIBS(brush, [gl glsl sw])
QF_PROCESS_NEED_LIBS(sprite, [gl glsl sw])

QF_PROCESS_NEED_LIBS(vid, [asm common gl glsl sdl sw sw32 svga x11])
QF_PROCESS_NEED_LIBS(qw, [client common sdl server], a)
QF_PROCESS_NEED_LIBS(nq, [client common sdl server], a)

if test -n "$CL_TARGETS"; then
	CD_TARGETS="libQFcd.la"
	SND_TARGETS="libQFsound.la"
	AUDIO_TARGETS="testsound\$(EXEEXT)"
	JOY_TARGETS="libQFjs.la"
else
	unset CDTYPE
	unset SOUND_TYPES
	CD_TARGETS=""
	JOY_TARGETS=""
	SND_TARGETS=""
	AUDIO_TARGETS=""
fi

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
QF_SUBST(PREFER_PIC)
QF_SUBST(PREFER_NON_PIC)
QF_SUBST(STATIC)

AC_ARG_WITH(static-plugins,
[  --with-static-plugins   build plugins into executable rather than separate],
	static_plugins="$withval", static_plugins=auto)
if test "x$static_plugins" = xauto; then
	if test "x$enable_shared" = xno -o "x$SYSTYPE" = xWIN32; then
		static_plugins=yes
	fi
fi
if test "x$static_plugins" = xyes; then
	QF_PROCESS_NEED_STATIC_PLUGINS(console, [server], [\$(top_builddir)/libs/console], [server])
	QF_PROCESS_NEED_STATIC_PLUGINS(console, [client], [\$(top_builddir)/libs/console], [client])

	QF_PROCESS_NEED_STATIC_PLUGINS(snd_output, [sdl mme sgi sun win dx oss alsa], [targets])
	QF_PROCESS_NEED_STATIC_PLUGINS(snd_render, [jack default], [renderer])
	QF_PROCESS_NEED_STATIC_PLUGINS(cd, [xmms sdl sgi win linux file])
	AC_DEFINE(STATIC_PLUGINS, 1, [Define this if you are building static plugins])
	if test -n "$SOUND_TYPES"; then
		SOUND_TYPES="$SOUND_TYPES (static)"
	fi
	if test -n "$CDTYPE"; then
		CDTYPE="$CDTYPE (static)"
	fi
else
	QF_PROCESS_NEED_PLUGINS(console, [server], [server])
	QF_PROCESS_NEED_PLUGINS(console, [client], [client])
	QF_PROCESS_NEED_PLUGINS(snd_output, [sdl mme sgi sun win dx oss alsa])
	QF_PROCESS_NEED_PLUGINS(snd_render, [jack default])
	QF_PROCESS_NEED_PLUGINS(cd, [xmms sdl sgi win linux file])
fi

dnl Do not use -module here, it belongs in makefile.am due to automake
dnl needing it there to work correctly

QF_SUBST(HW_TARGETS)
QF_SUBST(NQ_TARGETS)
QF_SUBST(NQ_DESKTOP_DATA)
QF_SUBST(QTV_TARGETS)
QF_SUBST(QW_TARGETS)
QF_SUBST(QW_DESKTOP_DATA)
QF_SUBST(CD_TARGETS)
QF_SUBST(JOY_TARGETS)
QF_SUBST(SND_TARGETS)
QF_SUBST(AUDIO_TARGETS)
QF_SUBST(VID_MODEL_TARGETS)
QF_SUBST(VID_REND_TARGETS)
QF_SUBST(VID_REND_NOINST_TARGETS)
QF_SUBST(VID_TARGETS)

QF_DEPS(BSP2IMG,
	[],
	[$(top_builddir)/libs/image/libQFimage.la
	 $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFBSP,
	[-I$(top_srcdir)/tools/qfbsp/include],
	[$(top_builddir)/libs/models/libQFmodels.la
	 $(top_builddir)/libs/image/libQFimage.la
	 $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFCC,
	[-I$(top_srcdir)/tools/qfcc/include],
	[$(top_builddir)/libs/gamecode/libQFgamecode.la
	 $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFLIGHT,
	[-I$(top_srcdir)/tools/qflight/include],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFLMP,
	[],
	[$(top_builddir)/libs/image/libQFimage.la
	 $(top_builddir)/libs/util/libQFutil.la],
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
	[$(top_builddir)/libs/ruamoko/libQFruamoko.la
	 $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(CARNE,
	[],
	[$(top_builddir)/libs/gib/libQFgib.la
	 $(top_builddir)/libs/ruamoko/libQFruamoko.la
	 $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(PAK,
	[],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(WAD,
	[],
	[$(top_builddir)/libs/image/libQFimage.la
	 $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(WAV,
	[],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
