include $(GNUSTEP_MAKEFILES)/common.make

BUNDLE_NAME=		MapEdit
BUNDLE_EXTENSION=	.forgeb
#
# We don't install this bundle, it goes inside the app.
#
BUNDLE_INSTALL_DIR=	none
MapEdit_STANDARD_INSTALL= no

MapEdit_RESOURCE_FILES= \
	MapEdit.gorm

MapEdit_OBJC_FILES=	\
	CameraView.m Clipper.m EntityClass.m KeypairView.m PopScrollView.m ZView.m render.m

MapEdit_HEADERS= \
	EntityClass.h

MapEdit_PRINCIPAL_CLASS= \
	MapEdit

-include GNUmakefile.preamble

include $(GNUSTEP_MAKEFILES)/bundle.make

-include GNUmakefile.postamble
