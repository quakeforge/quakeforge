dnl ==================================================================
dnl Find out what to build
dnl ==================================================================

QF_WITH_TARGETS(
	clients,
	[compile clients],
	[fbdev,sdl,svga,win,x11],dummy
)
QF_WITH_TARGETS(
	servers,
	[compile dedicated servers],
	[master,nq,qw,qtv],dummy
)
QF_WITH_TARGETS(
	tools,
	[compile qf tools],
	[bsp2img,carne,gsc,pak,qfbsp,qfcc,qflight,qflmp,qfmodelgen,qfspritegen,qfvis,qwaq,wad,wav],dummy
)

unset CL_TARGETS
HW_TARGETS=""
QTV_TARGETS=""
QW_TARGETS=""
NQ_TARGETS=""
QWAQ_TARGETS=""
QW_DESKTOP_DATA=""
NQ_DESKTOP_DATA=""

BSP2IMG_TARGETS=""
CARNE_TARGETS=""
PAK_TARGETS=""
QFBSP_TARGETS=""
QFCC_TARGETS=""
QFLIGHT_TARGETS=""
QFLMP_TARGETS=""
QFMODELGEN_TARGETS=""
QFSPRITEGEN_TARGETS=""
QFVIS_TARGETS=""
WAD_TARGETS=""
WAV_TARGETS=""
VKGEN_TARGETS=""

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
		VID_TARGETS="$VID_TARGETS libs/video/targets/libQFfbdev.la"
		QF_NEED(vid_render, [sw])
		QF_NEED(render, [sw])
		QF_NEED(models, [sw])
		QF_NEED(alias, [sw])
		QF_NEED(brush, [sw])
		QF_NEED(iqm, [sw])
		QF_NEED(sprite, [sw])
		if test "x$ASM_ARCH" = "xyes"; then
			QF_NEED(swrend, [asm])
		fi
		QF_NEED(vid, [common])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
		QF_NEED(libs,[util gamecode ruamoko gib audio image models video console net qw client])
	fi
fi
if test "x$HAVE_X" = xyes; then
	if test "x$ENABLE_clients_glx" = xyes; then
		QF_NEED(vid, [common x11])
	fi
	if test "x$ENABLE_clients_glslx" = xyes; then
		QF_NEED(vid, [common x11])
	fi
	if test "x$ENABLE_clients_x11" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-x11\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-x11\$(EXEEXT)"
		QWAQ_TARGETS="$QWAQ_TARGETS ruamoko/qwaq/qwaq-x11\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA desktop/quakeforge-qw-x11.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA desktop/quakeforge-nq-x11.desktop"
		CL_TARGETS="$CL_TARGETS X11"
		VID_TARGETS="$VID_TARGETS libs/video/targets/libQFx11.la"
		if test "$HAVE_VULKAN" = "yes"; then
			QF_NEED(vid_render, [vulkan])
			QF_NEED(render, [vulkan])
			QF_NEED(models, [vulkan])
			QF_NEED(alias, [vulkan])
			QF_NEED(brush, [vulkan])
			QF_NEED(iqm, [vulkan])
			QF_NEED(sprite, [vulkan])
		fi
		QF_NEED(vid_render, [sw gl glsl])
		QF_NEED(render, [sw gl glsl])
		QF_NEED(models, [sw gl glsl])
		QF_NEED(alias, [sw gl glsl])
		QF_NEED(brush, [sw gl glsl])
		QF_NEED(iqm, [sw gl glsl])
		QF_NEED(sprite, [sw gl glsl])
		if test "x$ASM_ARCH" = "xyes"; then
			QF_NEED(swrend, [asm])
		fi
		QF_NEED(vid, [common x11])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
		QF_NEED(libs,[util gamecode ruamoko gib audio image models video console net qw client])
	fi
fi
if test "x$HAVE_SDL" = xyes; then
	if test "x$ENABLE_clients_sdl" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-sdl\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-sdl\$(EXEEXT)"
		QW_DESKTOP_DATA="$QW_DESKTOP_DATA desktop/quakeforge-qw-sdl.desktop"
		NQ_DESKTOP_DATA="$NQ_DESKTOP_DATA desktop/quakeforge-nq-sdl.desktop"
		CL_TARGETS="$CL_TARGETS SDL"
		VID_TARGETS="$VID_TARGETS libs/video/targets/libQFsdl.la"
		QF_NEED(vid_render, [sw gl glsl])
		QF_NEED(render, [sw gl glsl])
		QF_NEED(models, [sw gl glsl])
		QF_NEED(alias, [sw gl glsl])
		QF_NEED(brush, [sw gl glsl])
		QF_NEED(iqm, [sw gl glsl])
		QF_NEED(sprite, [sw gl glsl])
		if test "x$ASM_ARCH" = "xyes"; then
			QF_NEED(swrend, [asm])
		fi
		QF_NEED(vid, [common sdl])
		QF_NEED(qw, [client common sdl])
		QF_NEED(nq, [client common sdl])
		QF_NEED(console, [client])
		QF_NEED(libs,[util gamecode ruamoko gib audio image models video console net qw client])
	fi
fi
if test "x$HAVE_SVGA" = xyes; then
	if test "x$ENABLE_clients_svga" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-svga\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-svga\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS SVGAlib"
		VID_TARGETS="$VID_TARGETS libs/video/targets/libQFsvga.la"
		QF_NEED(vid_render, [sw])
		QF_NEED(render, [sw])
		QF_NEED(models, [sw])
		QF_NEED(alias, [sw])
		QF_NEED(brush, [sw])
		QF_NEED(iqm, [sw])
		QF_NEED(sprite, [sw])
		if test "x$ASM_ARCH" = "xyes"; then
			QF_NEED(swrend, [asm])
		fi
		QF_NEED(vid, [common svga])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
		QF_NEED(libs,[util gamecode ruamoko gib audio image models video console net qw client])
	fi
fi
if test "x$mingw" = xyes; then
	if test "x$ENABLE_clients_win" = xyes; then
		QW_TARGETS="$QW_TARGETS qw-client-win\$(EXEEXT)"
		NQ_TARGETS="$NQ_TARGETS nq-win\$(EXEEXT)"
		CL_TARGETS="$CL_TARGETS WIN"
		VID_TARGETS="$VID_TARGETS libs/video/targets/libQFwin.la"
		if test "$HAVE_VULKAN" = "yes"; then
			QF_NEED(vid_render, [vulkan])
			QF_NEED(render, [vulkan])
			QF_NEED(models, [vulkan])
			QF_NEED(alias, [vulkan])
			QF_NEED(brush, [vulkan])
			QF_NEED(iqm, [vulkan])
			QF_NEED(sprite, [vulkan])
		fi
		QF_NEED(vid_render, [sw gl glsl])
		QF_NEED(models, [sw gl glsl])
		QF_NEED(alias, [sw gl glsl])
		QF_NEED(brush, [sw gl glsl])
		QF_NEED(iqm, [sw gl glsl])
		QF_NEED(sprite, [sw gl glsl])
		if test "x$ASM_ARCH" = "xyes"; then
			QF_NEED(swrend, [asm])
		fi
		QF_NEED(vid, [common win])
		QF_NEED(qw, [client common])
		QF_NEED(nq, [client common])
		QF_NEED(console, [client])
		QF_NEED(libs,[util gamecode ruamoko gib audio image models video console net qw client])
	fi
fi

unset SV_TARGETS
if test "x$ENABLE_servers_nq" = xyes; then
	NQ_TARGETS="nq-server\$(EXEEXT) $NQ_TARGETS"
	SV_TARGETS="$SV_TARGETS nq"
	QF_NEED(nq, [common server])
	QF_NEED(console, [server])
	QF_NEED(top, [nq])
	QF_NEED(libs,[util gamecode ruamoko gib image models console net])
fi
if test "x$ENABLE_servers_qtv" = xyes; then
	QTV_TARGETS="qtv-server\$(EXEEXT) $QTV_TARGETS"
	SV_TARGETS="$SV_TARGETS qtv-server"
#	QF_NEED(qtv, [common server])
	QF_NEED(console, [server])
	QF_NEED(top, [qtv])
	QF_NEED(libs,[util models console net qw])
fi
if test "x$ENABLE_servers_master" = xyes; then
	HW_TARGETS="hw-master\$(EXEEXT) $HW_TARGETS"
	QW_TARGETS="qw-master\$(EXEEXT) $QW_TARGETS"
	SV_TARGETS="$SV_TARGETS master"
	QF_NEED(top, [hw qw])
	QF_NEED(libs,[util console net qw])
fi
if test "x$ENABLE_servers_qw" = xyes; then
	QW_TARGETS="qw-server\$(EXEEXT) $QW_TARGETS"
	SV_TARGETS="$SV_TARGETS qw"
	QF_NEED(qw, [common server])
	QF_NEED(console, [server])
	QF_NEED(top, [qw])
	QF_NEED(libs,[util gamecode ruamoko gib models console net qw])
fi

if test "x$ENABLE_tools_bsp2img" = xyes; then
	BSP2IMG_TARGETS="bsp2img\$(EXEEXT)"
	QF_NEED(tools,[bsp2img])
	QF_NEED(libs,[image util])
fi
if test "x$ENABLE_tools_carne" = xyes; then
	CARNE_TARGETS="carne\$(EXEEXT)"
	QF_NEED(tools,[carne])
	QF_NEED(libs,[gib ruamoko gamecode util])
fi
if test "x$ENABLE_tools_pak" = xyes; then
	PAK_TARGETS="pak\$(EXEEXT)"
	QF_NEED(tools,[pak])
	QF_NEED(libs,[util])
fi
if test "x$ENABLE_tools_qfbsp" = xyes; then
	QFBSP_TARGETS="qfbsp\$(EXEEXT)"
	QF_NEED(tools,[qfbsp])
	QF_NEED(libs,[models image util])
fi
if test "x$ENABLE_tools_qfcc" = xyes; then
	QFCC_TARGETS="qfcc\$(EXEEXT) qfprogs\$(EXEEXT)"
	QF_NEED(tools,[qfcc])
	QF_NEED(libs,[gamecode util])
fi
if test "x$ENABLE_tools_qflight" = xyes; then
	QFLIGHT_TARGETS="qflight\$(EXEEXT)"
	QF_NEED(tools,[qflight])
	QF_NEED(libs,[util])
fi
if test "x$ENABLE_tools_qflmp" = xyes; then
	QFLMP_TARGETS="qflmp\$(EXEEXT)"
	QF_NEED(tools,[qflmp])
	QF_NEED(libs,[util])
fi
if test "x$ENABLE_tools_qfmodelgen" = xyes; then
	QFMODELGEN_TARGETS="qfmodelgen\$(EXEEXT)"
	QF_NEED(tools,[qfmodelgen])
	QF_NEED(libs,[util])
fi
if test "x$ENABLE_tools_qfspritegen" = xyes; then
	QFSPRITEGEN_TARGETS="qfspritegen\$(EXEEXT)"
	QF_NEED(tools,[qfspritegen])
	QF_NEED(libs,[util])
fi
if test "x$ENABLE_tools_qfvis" = xyes; then
	QFVIS_TARGETS="qfvis\$(EXEEXT)"
	QF_NEED(tools,[qfvis])
	QF_NEED(libs,[util])
fi
if test "x$ENABLE_tools_qwaq" = xyes; then
	if test "x$HAVE_NCURSES" == "xyes" -a "x$HAVE_PANEL" = xyes -a "x$HAVE_PTHREAD" = xyes; then
		QWAQ_TARGETS="$QWAQ_TARGETS ruamoko/qwaq/qwaq-curses\$(EXEEXT)"
		dnl FIXME move key code (maybe to ui?)
		QF_NEED(vid, [common])
	fi
	if test "x$HAVE_PTHREAD" = xyes; then
		QWAQ_TARGETS="$QWAQ_TARGETS ruamoko/qwaq/qwaq-cmd\$(EXEEXT)"
	fi
	QF_NEED(tools,[qfcc])
	QF_NEED(ruamoko,[qwaq])
	QF_NEED(libs,[ruamoko gamecode util])
fi
if test "x$ENABLE_tools_wad" = xyes; then
	WAD_TARGETS="wad\$(EXEEXT)"
	QF_NEED(tools,[wad])
	QF_NEED(libs,[image util])
fi
if test "x$ENABLE_tools_wav" = xyes; then
	WAV_TARGETS="qfwavinfo\$(EXEEXT)"
	QF_NEED(tools,[wav])
	QF_NEED(libs,[util])
fi
if test "x$render_need_vulkan" = xyes; then
	VKGEN_TARGETS="vkgen.dat\$(EXEEXT)"
	QF_NEED(tools,[qfcc pak qwaq])
fi

QF_NEED(top, [libs hw nq qtv qw])

QF_PROCESS_NEED_LIST(tools,[bsp2img carne pak qfbsp qfcc qflight qflmp qfmodelgen qfspritegen qfvis wad wav])
QF_PROCESS_NEED_FUNC(tools,[bsp2img carne pak qfbsp qfcc qflight qflmp qfmodelgen qfspritegen qfvis wad wav], QF_NEED(top,tools))

QF_PROCESS_NEED_LIST(libs,[util gamecode ruamoko gib audio image models video console net qw client])

QF_PROCESS_NEED_LIST(ruamoko,[qwaq])

if test "$ENABLE_tools_qfcc" = "yes" -a "$ENABLE_tools_pak" = "yes"; then
	QF_NEED(top, [ruamoko])
	qfac_qfcc_include_qf="\$(qfcc_include_qf)"
	qfac_qfcc_include_qf_input="\$(qfcc_include_qf_input)"
	qfac_qfcc_include_qf_progs="\$(qfcc_include_qf_progs)"
fi
QF_SUBST(qfac_qfcc_include_qf)
QF_SUBST(qfac_qfcc_include_qf_input)
QF_SUBST(qfac_qfcc_include_qf_progs)

if test x"${top_need_libs}" = xyes; then
	qfac_include_qf="\$(include_qf)"
	qfac_include_qf_gl="\$(include_qf_gl)"
	qfac_include_qf_glsl="\$(include_qf_glsl)"
	qfac_include_qf_input="\$(include_qf_input)"
	qfac_include_qf_math="\$(include_qf_math)"
	qfac_include_qf_plugin="\$(include_qf_plugin)"
	qfac_include_qf_progs="\$(include_qf_progs)"
	qfac_include_qf_scene="\$(include_qf_scene)"
	qfac_include_qf_simd="\$(include_qf_simd)"
	qfac_include_qf_ui="\$(include_qf_ui)"
	qfac_include_qf_vulkan="\$(include_qf_vulkan)"
fi
QF_SUBST(qfac_include_qf)
QF_SUBST(qfac_include_qf_gl)
QF_SUBST(qfac_include_qf_glsl)
QF_SUBST(qfac_include_qf_input)
QF_SUBST(qfac_include_qf_math)
QF_SUBST(qfac_include_qf_plugin)
QF_SUBST(qfac_include_qf_progs)
QF_SUBST(qfac_include_qf_scene)
QF_SUBST(qfac_include_qf_simd)
QF_SUBST(qfac_include_qf_ui)
QF_SUBST(qfac_include_qf_vulkan)

progs_gz=
if test "$HAVE_ZLIB" = "yes"; then
	progs_gz=".gz"
fi
QF_SUBST(progs_gz)

QF_PROCESS_NEED_LIST(top, [libs hw nq qtv qw tools ruamoko])

QF_PROCESS_NEED_LIBS(swrend, [asm])
QF_PROCESS_NEED_LIBS(render, [gl glsl sw vulkan], [libs/video/renderer])
QF_PROCESS_NEED_LIST(vid_render, [gl glsl sw vulkan])
QF_PROCESS_NEED_LIBS(models, [gl glsl sw vulkan], [libs/models])
QF_PROCESS_NEED_LIBS(alias, [gl glsl sw vulkan], [libs/models/alias])
QF_PROCESS_NEED_LIBS(brush, [gl glsl sw vulkan], [libs/models/brush])
QF_PROCESS_NEED_LIBS(iqm, [gl glsl sw vulkan], [libs/models/iqm])
QF_PROCESS_NEED_LIBS(sprite, [gl glsl sw vulkan], [libs/models/sprite])

QF_PROCESS_NEED_LIBS(input, [evdev], [libs/input])
QF_PROCESS_NEED_LIBS(vid, [common sdl svga win x11], [libs/video/targets])
QF_PROCESS_NEED_LIBS(qw, [client common sdl win server], [qw/source], a)
QF_PROCESS_NEED_LIBS(nq, [client common sdl win server], [nq/source], a)

if test -n "$CL_TARGETS"; then
	CD_TARGETS="libs/audio/libQFcd.la"
	SND_TARGETS="libs/audio/libQFsound.la"
	AUDIO_TARGETS="libs/audio/test/testsound\$(EXEEXT)"
else
	unset CDTYPE
	unset SOUND_TYPES
	CD_TARGETS=""
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
	AS_HELP_STRING([--with-static-plugins],
		[build plugins into executable rather than separate]),
	static_plugins="$withval", static_plugins=auto)
if test "x$static_plugins" = xauto; then
	if test "x$enable_shared" = xno -o "x$SYSTYPE" = xWIN32; then
		static_plugins=yes
	fi
fi
if test "x$static_plugins" = xyes; then
	QF_PROCESS_NEED_STATIC_PLUGINS(vid_render, [sw glsl gl vulkan], [libs/video/renderer])
	QF_PROCESS_NEED_STATIC_PLUGINS(console, [server], [libs/console], [server])
	QF_PROCESS_NEED_STATIC_PLUGINS(console, [client], [libs/console], [client])

	QF_PROCESS_NEED_STATIC_PLUGINS(snd_output, [sdl mme sgi sun win dx oss jack alsa], [libs/audio/targets])
	QF_PROCESS_NEED_STATIC_PLUGINS(snd_render, [default], [libs/audio/renderer])
	QF_PROCESS_NEED_STATIC_PLUGINS(cd, [xmms sdl sgi win linux file], [libs/audio])
	AC_DEFINE(STATIC_PLUGINS, 1, [Define this if you are building static plugins])
	if test -n "$SOUND_TYPES"; then
		SOUND_TYPES="$SOUND_TYPES (static)"
	fi
	if test -n "$CDTYPE"; then
		CDTYPE="$CDTYPE (static)"
	fi
else
	QF_PROCESS_NEED_PLUGINS(vid_render, [sw glsl gl vulkan], [libs/video/renderer])
	QF_PROCESS_NEED_PLUGINS(console, [server], [libs/console], [server])
	QF_PROCESS_NEED_PLUGINS(console, [client], [libs/console], [client])
	QF_PROCESS_NEED_PLUGINS(snd_output, [sdl mme sgi sun win dx oss jack alsa], [libs/audio/targets])
	QF_PROCESS_NEED_PLUGINS(snd_render, [default], [libs/audio/renderer])
	QF_PROCESS_NEED_PLUGINS(cd, [xmms sdl sgi win linux file], [libs/audio])
fi

dnl Do not use -module here, it belongs in makefile.am due to automake
dnl needing it there to work correctly

QF_SUBST(HW_TARGETS)
QF_SUBST(NQ_TARGETS)
QF_SUBST(NQ_DESKTOP_DATA)
QF_SUBST(QTV_TARGETS)
QF_SUBST(QWAQ_TARGETS)
QF_SUBST(QW_TARGETS)
QF_SUBST(QW_DESKTOP_DATA)
QF_SUBST(CD_TARGETS)
QF_SUBST(SND_TARGETS)
QF_SUBST(AUDIO_TARGETS)
QF_SUBST(VID_MODEL_TARGETS)
QF_SUBST(VID_REND_TARGETS)
QF_SUBST(VID_REND_NOINST_TARGETS)
QF_SUBST(VID_TARGETS)

QF_SUBST(BSP2IMG_TARGETS)
QF_SUBST(CARNE_TARGETS)
QF_SUBST(PAK_TARGETS)
QF_SUBST(QFBSP_TARGETS)
QF_SUBST(QFCC_TARGETS)
QF_SUBST(QFLIGHT_TARGETS)
QF_SUBST(QFLMP_TARGETS)
QF_SUBST(QFMODELGEN_TARGETS)
QF_SUBST(QFSPRITEGEN_TARGETS)
QF_SUBST(QFVIS_TARGETS)
QF_SUBST(WAD_TARGETS)
QF_SUBST(WAV_TARGETS)

QF_SUBST(VKGEN_TARGETS)

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
QF_DEPS(QFCC_TEST,
	[],
	[$(top_builddir)/libs/ruamoko/libQFruamoko.la
	 $(top_builddir)/libs/gamecode/libQFgamecode.la
	 $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFLIGHT,
	[-I$(top_srcdir)/tools/qflight/include],
	[$(top_builddir)/libs/gamecode/libQFgamecode.la
	 $(top_builddir)/libs/util/libQFutil.la],
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
QF_DEPS(QFSPRITEGEN,
	[],
	[$(top_builddir)/libs/image/libQFimage.la
	 $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QFVIS,
	[-I$(top_srcdir)/tools/qfvis/include],
	[$(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(QWAQ,
	[-I$(top_srcdir)/ruamoko/qwaq],
	[$(top_builddir)/libs/ruamoko/libQFruamoko.la
	 $(top_builddir)/libs/gamecode/libQFgamecode.la
	 $(top_builddir)/libs/util/libQFutil.la],
	[$(WIN32_LIBS)],
)
QF_DEPS(CARNE,
	[],
	[$(top_builddir)/libs/gib/libQFgib.la
	 $(top_builddir)/libs/ruamoko/libQFruamoko.la
	 $(top_builddir)/libs/gamecode/libQFgamecode.la
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
QF_DEPS(VKGEN,
	[],
	[],
	[],
)
