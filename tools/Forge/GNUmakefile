#
#	GNUmakefile
#
#	Main makefile for the Forge Quake Editor
#
#	Copyright (C) 2001 Jeff Teunissen <deek@dusknet.dhs.org>
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

#
# Main application
#
APP_NAME=		Forge
Forge_PRINCIPAL_CLASS=	Forge
Forge_APPLICATION_ICON=	Forge.tiff

#
# Additional libraries
#
ADDITIONAL_GUI_LIBS += 

#
# Resource files
#

# Languages we're localized for
Forge_LANGUAGES= \
	English

# We don't have a proper .gorm file yet, but we will
Forge_LOCALIZED_RESOURCE_FILES=	\
	Forge.gorm

Forge_RESOURCE_FILES= \
	ForgeInfo.plist \
	Images/DownArrow.tiff \
	Images/i_90d.tiff \
	Images/i_add.tiff \
	Images/i_brushes.tiff \
	Images/i_fliph.tiff \
	Images/i_flipv.tiff \
	Images/i_sub.tiff \
	Images/short.tiff \
	Images/tall.tiff \
	Images/UpArrow.tiff

#
# Header files
#
Forge_HEADERS= \
	Headers/CameraView.h \
	Headers/Clipper.h \
	Headers/Entity.h \
	Headers/EntityArray.h \
	Headers/EntityClass.h \
	Headers/Forge.h \
	Headers/InspectorControl.h \
	Headers/KeypairView.h \
	Headers/Map.h \
	Headers/PopScrollView.h \
	Headers/Preferences.h \
	Headers/Project.h \
	Headers/SetBrush.h \
	Headers/TexturePalette.h \
	Headers/TextureView.h \
	Headers/Things.h \
	Headers/UserPath.h \
	Headers/XYView.h \
	Headers/ZScrollView.h \
	Headers/ZView.h \
	Headers/cmdlib.h \
	Headers/mathlib.h \
	Headers/qedefs.h \
	Headers/render.h

#
# Class files
#
Forge_OBJC_FILES= \
	Clipper.m \
	Forge_main.m \
	Preferences.m \
	Project.m \
	Forge.m \
	Map.m \
	Things.m \
	misc.m \
	Entity.m \
	EntityArray.m \
	EntityClass.m \
	CameraView.m \
	XYView.m \
	KeypairView.m \
	InspectorControl.m \
	PopScrollView.m \
	TexturePalette.m \
	TextureView.m \
	ZScrollView.m \
	ZView.m \
	render.m
#	SetBrush.m \
#	UserPath.m \

#
# C files
#
Forge_C_FILES= \
	cmdlib.c

-include GNUmakefile.preamble
-include GNUmakefile.local

include $(GNUSTEP_MAKEFILES)/aggregate.make
include $(GNUSTEP_MAKEFILES)/application.make

-include GNUmakefile.postamble
