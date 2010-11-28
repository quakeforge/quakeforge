include GNUmakefile.find-makefiles

include $(GNUSTEP_MAKEFILES)/common.make

#
# Subprojects
#
SUBPROJECTS=

#
# Main application
#
PACKAGE_NAME=		QuakeEd
APP_NAME=		QuakeEd
QuakeEd_PRINCIPAL_CLASS=	QuakeEdApp
QuakeEd_APPLICATION_ICON=	

#
# Additional libraries
#
ADDITIONAL_GUI_LIBS += 

#
# Resource files
#
QuakeEd_MAIN_MODEL_FILE=	MapEdit.gorm
QuakeEd_RESOURCE_FILES= \
	MapEdit.gorm \
	help.txt \
	../../Images/DownArrow.tiff \
	../../Images/UpArrow.tiff \
	../../Images/i_90d.tiff \
	../../Images/i_add.tiff \
	../../Images/i_brushes.tiff \
	../../Images/i_fliph.tiff \
	../../Images/i_flipv.tiff \
	../../Images/i_quakeed.tiff \
	../../Images/i_sub.tiff \
	../../Images/short.tiff \
	../../Images/tall.tiff	

QuakeEd_LOCALIZED_RESOURCE_FILES=

# Languages we're localized for
QuakeEd_LANGUAGES= \
	English

#
# Header files
#
QuakeEd_HEADERS= \
	CameraView.h Clipper.h Dict.h Entity.h EntityClass.h \
	InspectorControl.h KeypairView.h Map.h PopScrollView.h Preferences.h \
	Project.h QuakeEd.h SetBrush.h Storage.h TexturePalette.h TextureView.h \
	Things.h XYView.h ZScrollView.h ZView.h render.h \
	\
	THING+NSArray.m

#
# Class files
#
QuakeEd_OBJC_FILES= \
	CameraView.m Clipper.m Dict.m Entity.m EntityClass.m \
	InspectorControl.m KeypairView.m Map.m PopScrollView.m Preferences.m \
	Project.m QuakeEd.m QuakeEd_main.m SetBrush.m Storage.m TexturePalette.m \
	TextureView.m Things.m XYView.m ZScrollView.m ZView.m render.m

#
# C files
#
QuakeEd_C_FILES=

-include GNUmakefile.preamble
-include GNUmakefile.local

include $(GNUSTEP_MAKEFILES)/aggregate.make
include $(GNUSTEP_MAKEFILES)/application.make

-include GNUmakefile.postamble

