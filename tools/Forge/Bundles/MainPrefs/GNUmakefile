include $(GNUSTEP_MAKEFILES)/common.make

BUNDLE_NAME=		MainPrefs
BUNDLE_EXTENSION=	.forgeb
#
# We don't install this bundle, it goes inside the app.
#
BUNDLE_INSTALL_DIR=	none
MainPrefs_STANDARD_INSTALL= no

MainPrefs_RESOURCE_FILES= \
	MainPrefs.gorm

MainPrefs_OBJC_FILES=	\
	MainPrefs.m

MainPrefs_HEADERS= \
	MainPrefs.h

MainPrefs_PRINCIPAL_CLASS= \
	MainPrefs

-include GNUmakefile.preamble

include $(GNUSTEP_MAKEFILES)/bundle.make

-include GNUmakefile.postamble
