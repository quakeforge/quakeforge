AUTOMAKE_OPTIONS= foreign

AM_CFLAGS= @PREFER_PIC@
AM_CPPFLAGS= -I$(top_srcdir)/include -DVK_NO_PROTOTYPES

vulkan_src = \
	command.c \
	device.c \
	instance.c \
	swapchain.c \
	util.c \
	vulkan_draw.c \
	vulkan_vid_common.c

noinst_LTLIBRARIES=	libvulkan.la
BUILT_SOURCES= $(shader_gen)

SUFFICES=.frag .vert .fc .vc .slc .glsl
.glsl.slc:
	sed -e 's/^/"/' -e 's/$$/\\n"/' $< > $@
.frag.fc:
	sed -e 's/^/"/' -e 's/$$/\\n"/' $< > $@
.vert.vc:
	sed -e 's/^/"/' -e 's/$$/\\n"/' $< > $@

libvulkan_la_SOURCES= $(vulkan_src)

EXTRA_DIST = $(vulkan_src) $(shader_src) namehack.h util.h
CLEANFILES= *.vc *.fc *.slc
