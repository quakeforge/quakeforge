include $(GNUSTEP_MAKEFILES)/common.make

OBJCFLAGS += -Wall

PACKAGE_NAME=		Forge
APP_NAME=		Forge
Forge_APPLICATION_ICON=	Forge.tiff

#
# Resource files
#
Forge_LANGUAGES=		English
# We don't have a .gorm file yet, but we will
Forge_LOCALIZED_RESOURCE_FILES=	Forge.gorm
Forge_RESOURCE_FILES= 	DownArrow.tiff i_90d.tiff i_add.tiff i_brushes.tiff \
			i_fliph.tiff i_flipv.tiff i_sub.tiff short.tiff \
			tall.tiff UpArrow.tiff

#
# Header files
#
Forge_HEADERS=	CameraView.h Clipper.h Entity.h EntityArray.h EntityClass.h \
		Forge.h InspectorControl.h KeypairView.h Map.h PopScrollView.h \
		Preferences.h Project.h SetBrush.h TexturePalette.h \
		TextureView.h Things.h UserPath.h XYView.h ZScrollView.h \
		ZView.h \
		render.h cmdlib.h mathlib.h

#
# Class files
#
Forge_OBJC_FILES=	CameraView.m Clipper.m Entity.m EntityArray.m \
			EntityClass.m Forge.m Forge_main.m InspectorControl.m \
			KeypairView.m Map.m PopScrollView.m Preferences.m \
			Project.m SetBrush.m TexturePalette.m TextureView.m \
			Things.m UserPath.m XYView.m ZScrollView.m ZView.m \
			misc.m render.m

#
# C files
#
Forge_C_FILES=		cmdlib.c mathlib.c

Forge_OTHER_SOURCES=	GNUmakefile

-include GNUmakefile.preamble

include $(GNUSTEP_MAKEFILES)/application.make

-include GNUmakefile.postamble

-include GNUmakefile.dependencies
