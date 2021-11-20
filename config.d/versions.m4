dnl Define the proper name and extra version numbers for package
NQ_VERSION=1.09
NQ_QSG_VERSION=1.0
QW_VERSION=2.40
QW_QSG_VERSION=2.0
RPMVERSION=`echo $PACKAGE_VERSION | tr - _`
QF_SUBST(RPMVERSION)

AC_DEFINE_UNQUOTED(NQ_VERSION,		"$NQ_VERSION",
	[Define this to the NetQuake standard version you support])
AC_DEFINE_UNQUOTED(NQ_QSG_VERSION,	"$NQ_QSG_VERSION",
	[Define this to the QSG standard version you support in NetQuake])
AC_DEFINE_UNQUOTED(QW_VERSION,		"$QW_VERSION",
	[Define this to the QuakeWorld standard version you support])
AC_DEFINE_UNQUOTED(QW_QSG_VERSION,	"$QW_QSG_VERSION",
	[Define this to the QSG standard version you support in QuakeWorld])

AC_ARG_ENABLE([version-info], AS_HELP_STRING([--enable-version-info=CURRENT:REVISION:AGE],
	[Override the value passed to libtool -version-info.]),
	[], [enable_version_info=1:0:0])
QUAKE_LIBRARY_VERSION_INFO=$enable_version_info
AC_SUBST([QUAKE_LIBRARY_VERSION_INFO])
