AC_MSG_RESULT([
	QuakeForge has been configured successfully.

	Build type         :$BUILD_TYPE
	Server support     :${SV_TARGETS:- no}
	Client support     :${CL_TARGETS:- no}
	Tools support      :${tools_list:- no}
	Sound support      :${SOUND_TYPES:- no} ${snd_output_default}
	CD Audio system    :${CDTYPE:- no} ${cd_default}
	IPv6 networking    : $NETTYPE_IPV6
	Compression support: gz=$HAVE_ZLIB ogg=$HAVE_VORBIS flac=$HAVE_FLAC png=$HAVE_PNG
	HTTP support       : ${CURL:-no}
	Compiler version   : $CCVER
	Compiler flags     : $CFLAGS
	SIMD Support       : $simd
	qfcc cpp invocation: $CPP_NAME

	Shared game data directory  : $sharepath
	Per-user game data directory: $userpath
	Plugin load directory       : $expanded_plugindir
	Global configuration file   : $globalconf
	User configuration file     : $userconf
	OpenGL dynamic lib          : $gl_driver
	libWildMidi Support         : $HAVE_WILDMIDI
	XDG support                 : $HAVE_XDG
])

if test -d $srcdir/.git; then
	echo "WARNING: Hackers at work, watch for falling bits of code."
	echo "(This is from a development git tree. Expect problems)"
	echo
fi
