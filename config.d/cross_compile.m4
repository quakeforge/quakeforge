if test "x$cross_compiling" = xyes; then
	AC_MSG_CHECKING(whether byte ordering is bigendian)
	AC_ARG_WITH(endian,
		AS_HELP_STRING([--with-endian=TYPE],
			[set endian of target system for cross-compiling.]
			[TYPE = little or big.]),
		endian="$withval",
	)
	case "x$endian" in
		xbig)
			AC_DEFINE(WORDS_BIGENDIAN)
			AC_MSG_RESULT(yes)
			;;
		xlittle)
			AC_MSG_RESULT(no)
			;;
		x)
			AC_MSG_RESULT(unspecified, use --with-endian={big,little})
			exit 1
			;;
		x*)
			AC_MSG_RESULT(unrecognized endianness)
			exit 1
			;;
	esac
else
	AC_C_BIGENDIAN
fi
