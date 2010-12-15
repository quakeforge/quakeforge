dnl ==================================================================
dnl Checks for sound
dnl ==================================================================

AC_CHECK_LIB(mme, waveOutOpen, HAVE_LIBMME=yes)

AC_ARG_ENABLE(alsa,
	[  --disable-alsa          disable checks for ALSA support])

AC_ARG_ENABLE(oss,
	[  --disable-oss           disable checks for OSS support])

AC_ARG_ENABLE(sound,
	[  --disable-sound         disable sound outright])

unset SOUND_TYPES

HAVE_ALSA=no
if test "x$enable_alsa" != "xno"; then
	if test "x$ac_cv_header_sys_asoundlib_h" = "xyes" -o \
		"x$ac_cv_header_alsa_asoundlib_h" = "xyes"; then
			AC_EGREP_CPP([QF_maGiC_VALUE],
			[
#include <alsa/asoundlib.h>
#if defined(SND_LIB_MAJOR) && defined(SND_LIB_MINOR) && \
    defined(SND_LIB_SUBMINOR)
#if SND_LIB_MAJOR > 0 || (SND_LIB_MAJOR == 0 && SND_LIB_MINOR >= 9 && \
			  SND_LIB_SUBMINOR >= 8)
QF_maGiC_VALUE
#endif
#endif
			],
			SOUND_TYPES="$SOUND_TYPES ALSA"
			ALSA_LIBS="-lasound"
			HAVE_ALSA=yes,
			AC_EGREP_CPP([QF_maGiC_VALUE],
				[
#include <sys/asoundlib.h>
#if defined(SND_LIB_MAJOR) && defined(SND_LIB_MINOR) && \
    defined(SND_LIB_SUBMINOR)
#if SND_LIB_MAJOR > 0 || (SND_LIB_MAJOR == 0 && SND_LIB_MINOR >= 9 && \
			  SND_LIB_SUBMINOR >= 8)
QF_maGiC_VALUE
#endif
#endif
				],
				SOUND_TYPES="$SOUND_TYPES ALSA"
				ALSA_LIBS="-lasound"
				HAVE_ALSA=yes
			)
	    )
	fi
fi
AC_SUBST(ALSA_LIBS)
AC_SUBST(HAVE_ALSA)

AC_ARG_ENABLE(jack,
[  --disable-jack          disable jack support],
)
HAVE_JACK=no
JACK_LIBS=""
if test "x$enable_jack" != "xno"; then
	if test "x$PKG_CONFIG" != "x"; then
		PKG_CHECK_MODULES([JACK], [jack], HAVE_JACK=yes, HAVE_JACK=no)
		if test "x$HAVE_JACK" = "xyes"; then
			AC_DEFINE(HAVE_JACK, 1, [Define if you have libjack])
			SOUND_TYPES="$SOUND_TYPES JACK"
		fi
	else
		AC_CHECK_LIB(jack, jack_client_open, HAVE_JACK=yes, HAVE_JACK=no,
					 [$LIBS])
		if test "x$HAVE_JACK" = "xyes"; then
			AC_CHECK_HEADER(jack/jack.h, HAVE_JACK=yes, HAVE_JACK=no)
			if test "x$HAVE_JACK" = "xyes"; then
				JACK_LIBS="-ljack"
				AC_DEFINE(HAVE_JACK, 1, [Define if you have libjack])
				SOUND_TYPES="$SOUND_TYPES JACK"
			fi
		fi
	fi
fi
AC_SUBST(JACK_LIBS)

dnl AC_ARG_ENABLE(samplerate,
dnl [  --disable-samplerate    disable libsamplerate support],
dnl )
HAVE_SAMPLERATE=no
SAMPLERATE_LIBS=""
if test "x$enable_samplerate" != "xno"; then
	if test "x$PKG_CONFIG" != "x"; then
		PKG_CHECK_MODULES([SAMPLERATE], [samplerate], HAVE_SAMPLERATE=yes,
						  HAVE_SAMPLERATE=no)
		if test "x$HAVE_SAMPLERATE" = "xyes"; then
			AC_DEFINE(HAVE_SAMPLERATE, 1, [Define if you have libsamplerate])
		fi
	else
		AC_CHECK_LIB(samplerate, src_callback_new, HAVE_SAMPLERATE=yes,
					 HAVE_SAMPLERATE=no, [$LIBS])
		if test "x$HAVE_SAMPLERATE" = "xyes"; then
			AC_CHECK_HEADER(samplerate.h, HAVE_SAMPLERATE=yes,
							HAVE_SAMPLERATE=no)
			if test "x$HAVE_SAMPLERATE" = "xyes"; then
				SAMPLERATE_LIBS="-lsamplerate"
				AC_DEFINE(HAVE_SAMPLERATE, 1,
						  [Define if you have libsamplerate])
			fi
		fi
	fi
fi
AC_SUBST(SAMPLERATE_LIBS)
dnl AM_CONDITIONAL(HAVE_SAMPLERATE, test "$HAVE_SAMPLERATE" = "yes")
if test "x$HAVE_SAMPLERATE" = "xno"; then
	AC_MSG_ERROR(libsamplerate is required but was not found)
fi

SOUND_TYPES="$SOUND_TYPES DISK"

dnl MME
if test "x$ac_cv_header_mme_mmsystem_h" = "xyes" -a \
		"x$HAVE_LIBMME" = "xyes"; then
	AC_EGREP_CPP([QF_maGiC_VALUE],
		[
#include <mme/mmsystem.h>
#ifdef WAVE_OPEN_SHAREABLE
QF_maGiC_VALUE
#endif
		],
		SOUND_TYPES="$SOUND_TYPES MME"
		MME_LIBS="-lmme"
	)
fi
AC_SUBST(MME_LIBS)

dnl OSS
HAVE_OSS=no
if test "x$enable_oss" != "xno"; then
	if test "x$ac_cv_header_sys_soundcard_h" = "xyes" -o \
		"x$ac_cv_header_machine_soundcard_h" = "xyes" -o \
		"x$ac_cv_header_linux_soundcard_h" = "xyes"; then
	AC_EGREP_CPP([QF_maGiC_VALUE],
		[
#include <sys/soundcard.h>
#ifdef SNDCTL_DSP_SETTRIGGER
QF_maGiC_VALUE
#endif
		],
		SOUND_TYPES="$SOUND_TYPES OSS"
		HAVE_OSS=yes
		OSS_LIBS=
		,
		AC_EGREP_CPP([QF_maGiC_VALUE],
			[
#include <linux/soundcard.h>
#ifdef SNDCTL_DSP_SETTRIGGER
QF_maGiC_VALUE
#endif
			],
			SOUND_TYPES="$SOUND_TYPES OSS"
			HAVE_OSS=yes
			OSS_LIBS=
			,
			AC_EGREP_CPP([QF_maGiC_VALUE],
				[
#include <machine/soundcard.h>
#ifdef SNDCTL_DSP_SETTRIGGER
QF_maGiC_VALUE
#endif
				],
				SOUND_TYPES="$SOUND_TYPES OSS"
				HAVE_OSS=yes
				OSS_LIBS=
			)
		)
	)
    fi
fi
AC_SUBST(HAVE_OSS)
AC_SUBST(OSS_LIBS)

dnl SDL digital audio
if test "x$HAVE_SDL_AUDIO" = "xyes"; then
	SOUND_TYPES="$SOUND_TYPES SDL"
fi

dnl SGI
if test "x$ac_cv_header_dmedia_audio_h" = "xyes"; then
   AC_EGREP_CPP([QF_maGiC_VALUE],
   		[
#include <dmedia/audio.h>
#ifdef AL_SAMPLE_16
# ifdef AL_RATE
QF_maGiC_VALUE
# endif
#endif
		],
		SOUND_TYPES="$SOUND_TYPES SGI"
		SGISND_LIBS="-laudio")
fi
AC_SUBST(SGISND_LIBS)

dnl Sun
if test "x$ac_cv_header_sys_audioio_h" = "xyes"; then
	AC_EGREP_CPP([QF_maGiC_VALUE],
		[
#include <sys/audioio.h>
#ifdef AUDIO_SETINFO
QF_maGiC_VALUE
#endif
		],
		SOUND_TYPES="$SOUND_TYPES SUN"
	)
fi

dnl Win32
if test "x$ac_cv_header_windows_h" = "xyes"; then
	SOUND_TYPES="$SOUND_TYPES Win32"
	WINSND_LIBS="-lwinmm"
	if test "x$ac_cv_header_dsound_h" = "xyes"; then
	AC_EGREP_CPP([QF_maGiC_VALUE],
		[
#include <windows.h>
#include <dsound.h>
#ifdef GMEM_MOVEABLE
# ifdef DirectSoundEnumerate
QF_maGiC_VALUE
# endif
#endif
		],
		SOUND_TYPES="$SOUND_TYPES DirectX"
	)
	fi
fi
AC_SUBST(WINSND_LIBS)

if test "x$enable_sound" = "xno"; then
	unset SOUND_TYPES
fi
