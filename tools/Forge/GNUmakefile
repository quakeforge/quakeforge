#
#	GNUmakefile
#
#	Main makefile for the Forge Quake Editor
#
#	Copyright (C) 2001 Jeff Teunissen <deek@d2dc.net>
#
#	This Makefile is free software; you can redistribute it and/or
#	modify it under the terms of the GNU General Public License
#	as published by the Free Software Foundation; either version 2
#	of the License, or (at your option) any later version.
#
#	This program is distributed in the hope that it will be useful,
#	but WITHOUT ANY WARRANTY; without even the implied warranty of
#	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#	See the GNU General Public License for more details.
#
#	You should have received a copy of the GNU General Public License
#	along with this program; if not, write to:
#
#		Free Software Foundation, Inc.
#		59 Temple Place - Suite 330
#		Boston, MA  02111-1307, USA
#
#	$Id$

include $(GNUSTEP_MAKEFILES)/common.make

#
# Subprojects
#
SUBPROJECTS=	\
	Bundles/MainPrefs

#
# Main application
#
PACKAGE_NAME=		Forge
APP_NAME=		Forge
Forge_PRINCIPAL_CLASS=	NSApplication
Forge_APPLICATION_ICON=	Forge.tiff

#
# Additional libraries
#
ADDITIONAL_GUI_LIBS += 

#
# Resource files
#
Forge_MAIN_MODEL_FILE=	Forge.gorm
Forge_RESOURCE_FILES= \
	Bundles/MainPrefs/MainPrefs.forgeb \
	Forge.gorm \
	Forge.tiff \
	ForgeDocument.tiff \
	ForgeInfo.plist

Forge_LOCALIZED_RESOURCE_FILES= \
	Localizable.strings

# Languages we're localized for
Forge_LANGUAGES= \
	English

#
# Header files
#
Forge_HEADERS= \
	Config.h \
	BundleController.h \
	Controller.h \
	PrefsPanel.h \
	PrefsController.h

#
# Class files
#
Forge_OBJC_FILES= \
	BundleController.m \
	Controller.m \
	PrefsPanel.m \
	PrefsController.m \
	main.m

#
# C files
#
Forge_C_FILES=

-include GNUmakefile.preamble
-include GNUmakefile.local

include $(GNUSTEP_MAKEFILES)/aggregate.make
include $(GNUSTEP_MAKEFILES)/application.make

-include GNUmakefile.postamble
