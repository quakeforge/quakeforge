#
#        Borland C++ makefile for Quakeforge (newtree)
#
#        Copyright (C) 1999,2000  Jukka Sorjonen.
#        Please see the file "AUTHORS" for a list of contributors
#
#        This program is free software; you can redistribute it and/or
#        modify it under the terms of the GNU General Public License
#        as published by the Free Software Foundation; either version 2
#        of the License, or (at your option) any later version.
#
#        This program is distributed in the hope that it will be useful,
#        but WITHOUT ANY WARRANTY; without even the implied warranty of
#        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
#
#        See the GNU General Public License for more details.
#
#        You should have received a copy of the GNU General Public License
#        along with this program; if not, write to:
#
#                Free Software Foundation, Inc.
#                59 Temple Place - Suite 330
#                Boston, MA  02111-1307, USA
#
#

.AUTODEPEND

#
# Borland C++ tools
#
IMPLIB  = Implib
BCC32   = Bcc32
BCC32I  = Bcc32i
#TLINK32 = TLink32
TLINK32 = Ilink32
ILINK32 = Ilink32
TLIB    = TLib
BRC32   = Brc32
TASM32  = Tasm32

#
# Options
#

# Where quakeforge source is located
QFROOT = D:\PROJECT\QUAKE1\NEWTREE

# Complier root directory
CROOT = D:\BORLAND\BCC55
# For 5.02
#CROOT = D:\BC5
# For C++ Builder
#CROOT = D:\PROGRA~1\BORLAND\CBUILDER5

# Where you want to put those .obj files
#OBJS = $(QFROOT)\TARGETS\QW_CLIENT
OBJS = $(QFROOT)\SOURCE

# ... and final exe
#EXE = $(QFROOT)\TARGETS
EXE = $(QFROOT)

# Path to your Direct-X libraries and includes
DIRECTXSDK=D:\project\dx7sdk
# Path to your SDL SDK libraries and includes
SDLSDK=d:\project\SDL-1.1.6
# Path to ZLIB source code
ZLIB=D:\PROJECT\ZLIB

# end of system dependant stuffs

SYSLIBS = $(CROOT)\LIB
MISCLIBS = $(DIRECTXSDK)\lib\borland
LIBS=$(SYSLIBS);$(MISCLIBS)

SYSINCLUDE = $(CROOT)\INCLUDE
QFINCLUDES = $(QFROOT)\INCLUDE\WIN32\BC;$(QFROOT)\INCLUDE\WIN32;$(QFROOT)\INCLUDE
MISCINCLUDES = $(DIRECTXSDK)\include;$(SDLSDK)\include;$(ZLIB)

INCLUDES = $(QFINCLUDES);$(SYSINCLUDE);$(MISCINCLUDES)

DEFINES=WIN32=1;WINDOWS=1;_WINDOWS=1;_WIN32=1;HAVE_CONFIG_H=1;USE_INTEL_ASM=1;HAVE_FNMATCH_H=1

# for releases
DEBUGOPTS = -k- -vi
# for debugging
#DEBUGOPTS = -y -v
# -xp -xs -o

# no optimizations - for debugging
#OPT = -a -O-S -Od
# for basic optimizations for 386
OPT = -3 -Oc -Oi -Ov -a4
# for Pentium
#OPT = -5 -Oc -Oi -Ov -a4 -OS
# for Pentium Pro and higher
#OPT = -6 -Oc -Oi -Ov -a4 -OS
# Testing purposes
#OPT = -6 -Oc -Oi -Ov -a4 -Og -Oc -Ol -Ob -Oe -Om -Op

# disable warnings, for cleaner compile
WARNS = -w-
# for debugging
#WARNS = -w

COMPOPTS = $(DEBUGOPTS) $(OPT) $(WARNS) -R -WM -H-
#-He- -f -ff -fp-

# for normal releases
LINKOPTS = -w-dup -w-dpl -Tpe -aa -V4.0 -c -Gn -Gz -x -L$(LIBS)
# for debugging
#LINKOPTS = -w -v -w-dup -w-dpl -Tpe -aa -V4.0 -c -Gn -Gz -m -M -s -L$(LIBS)
# -Gm

# MASM
ASSEMBLER = ML
ASMOUT = $(QFROOT)\source
ASMIN = /Fo$(OBJS)
#ASMOPTS=/nologo /c /Cp /Zi /H64
ASMOPTS=/nologo /c /Cp
#/Cx /Zi /Zd /H64
EXT1=.asm
EXT2=.obj

# TASM32
#ASSEMBLER = $(TASM32)
#ASMIN = $(QFROOT)\source
#ASMOUT = ,$(QFROOT)\source
#ASMOPTS = /ml
#EXT1=.obj
#EXT2=.asm

DEPEND = \
   $(OBJS)\hash.obj\
   $(OBJS)\pcx.obj\
   $(OBJS)\vid.obj\
   $(OBJS)\joy_null.obj\
   $(OBJS)\locs.obj\
   $(ZLIB)\zlib.lib\
   $(OBJS)\sdl_main.obj\
   $(OBJS)\model.obj\
   $(OBJS)\model_brush.obj\
   $(OBJS)\model_alias.obj\
   $(OBJS)\model_sprite.obj\
   $(OBJS)\sw_model_alias.obj\
   $(OBJS)\sw_model_brush.obj\
   $(OBJS)\sw_model_sprite.obj\
   $(OBJS)\teamplay.obj\
   $(SDLSDK)\lib\sdl.lib\
   $(OBJS)\vid_sdl.obj\
   $(OBJS)\sw_skin.obj\
   $(OBJS)\sw_view.obj\
   $(OBJS)\r_view.obj\
   $(OBJS)\r_sprite.obj\
   $(OBJS)\r_part.obj\
   $(OBJS)\r_vars.obj\
   $(OBJS)\r_surf.obj\
   $(OBJS)\r_sky.obj\
   $(OBJS)\screen.obj\
   $(OBJS)\d_edge.obj\
   $(OBJS)\r_main.obj\
   $(OBJS)\r_light.obj\
   $(OBJS)\r_efrag.obj\
   $(OBJS)\r_edge.obj\
   $(OBJS)\r_draw.obj\
   $(OBJS)\r_bsp.obj\
   $(OBJS)\r_alias.obj\
   $(OBJS)\r_aclip.obj\
   $(OBJS)\draw.obj\
   $(OBJS)\d_zpoint.obj\
   $(OBJS)\d_vars.obj\
   $(OBJS)\d_surf.obj\
   $(OBJS)\d_sprite.obj\
   $(OBJS)\d_sky.obj\
   $(OBJS)\d_scan.obj\
   $(OBJS)\d_polyse.obj\
   $(OBJS)\d_part.obj\
   $(OBJS)\d_modech.obj\
   $(OBJS)\d_init.obj\
   $(OBJS)\d_fill.obj\
   $(OBJS)\r_misc.obj\
   $(OBJS)\worlda.obj\
   $(OBJS)\d_varsa.obj\
   $(OBJS)\sys_x86.obj\
   $(OBJS)\surf8.obj\
   $(OBJS)\surf16.obj\
   $(OBJS)\snd_mixa.obj\
   $(OBJS)\r_varsa.obj\
   $(OBJS)\r_edgea.obj\
   $(OBJS)\r_drawa.obj\
   $(OBJS)\r_aliasa.obj\
   $(OBJS)\r_aclipa.obj\
   $(OBJS)\math.obj\
   $(OBJS)\cl_math.obj\
   $(OBJS)\d_draw.obj\
   $(OBJS)\d_scana.obj\
   $(OBJS)\d_polysa.obj\
   $(OBJS)\d_parta.obj\
   $(OBJS)\d_draw16.obj\
   $(OBJS)\d_spr8.obj\
   $(OBJS)\borland.obj\
   $(DIRECTXSDK)\lib\borland\dxguid.lib\
   $(OBJS)\buildnum.obj\
   $(OBJS)\checksum.obj\
   $(OBJS)\com.obj\
   $(OBJS)\info.obj\
   $(OBJS)\sizebuf.obj\
   $(OBJS)\msg.obj\
   $(OBJS)\va.obj\
   $(OBJS)\qargs.obj\
   $(OBJS)\quakefs.obj\
   $(OBJS)\qendian.obj\
   $(OBJS)\quakeio.obj\
   $(OBJS)\net_udp.obj\
   $(OBJS)\zone.obj\
   $(OBJS)\pmovetst.obj\
   $(OBJS)\pmove.obj\
   $(OBJS)\net_com.obj\
   $(OBJS)\net_chan.obj\
   $(OBJS)\cmd.obj\
   $(OBJS)\mdfour.obj\
   $(OBJS)\cvar.obj\
   $(OBJS)\crc.obj\
   $(OBJS)\fnmatch.obj\
   $(OBJS)\sys_win.obj\
   $(OBJS)\snd_win.obj\
   $(OBJS)\cd_sdl.obj\
   $(OBJS)\in_sdl.obj\
   $(OBJS)\cl_sys_sdl.obj\
   $(OBJS)\cl_slist.obj\
   $(OBJS)\mathlib.obj\
   $(OBJS)\nonintel.obj\
   $(OBJS)\menu.obj\
   $(OBJS)\keys.obj\
   $(OBJS)\console.obj\
   $(OBJS)\wad.obj\
   $(OBJS)\snd_mix.obj\
   $(OBJS)\snd_mem.obj\
   $(OBJS)\snd_dma.obj\
   $(OBJS)\skin.obj\
   $(OBJS)\cl_cam.obj\
   $(OBJS)\cl_tent.obj\
   $(OBJS)\cl_pred.obj\
   $(OBJS)\cl_parse.obj\
   $(OBJS)\cl_misc.obj\
   $(OBJS)\cl_main.obj\
   $(OBJS)\cl_input.obj\
   $(OBJS)\cl_ents.obj\
   $(OBJS)\cl_demo.obj\
   $(OBJS)\cl_cvar.obj\
   $(OBJS)\cl_cmd.obj\
   $(OBJS)\sbar.obj

$(EXE)\qf-client-sdl.exe : $(DEPEND)
  $(TLINK32) /v @&&|
 $(LINKOPTS) +
$(CROOT)\LIB\c0w32.obj+
$(OBJS)\hash.obj+
$(OBJS)\pcx.obj+
$(OBJS)\vid.obj+
$(OBJS)\joy_null.obj+
$(OBJS)\locs.obj+
$(ZLIB)\zlib.lib+
$(OBJS)\sdl_main.obj+
$(OBJS)\model.obj+
$(OBJS)\model_brush.obj+
$(OBJS)\model_alias.obj+
$(OBJS)\model_sprite.obj+
$(OBJS)\sw_model_alias.obj+
$(OBJS)\sw_model_brush.obj+
$(OBJS)\sw_model_sprite.obj+
$(OBJS)\teamplay.obj+
$(OBJS)\vid_sdl.obj+
$(OBJS)\sw_skin.obj+
$(OBJS)\sw_view.obj+
$(OBJS)\r_view.obj+
$(OBJS)\r_sprite.obj+
$(OBJS)\r_part.obj+
$(OBJS)\r_vars.obj+
$(OBJS)\r_surf.obj+
$(OBJS)\r_sky.obj+
$(OBJS)\screen.obj+
$(OBJS)\d_edge.obj+
$(OBJS)\r_main.obj+
$(OBJS)\r_light.obj+
$(OBJS)\r_efrag.obj+
$(OBJS)\r_edge.obj+
$(OBJS)\r_draw.obj+
$(OBJS)\r_bsp.obj+
$(OBJS)\r_alias.obj+
$(OBJS)\r_aclip.obj+
$(OBJS)\draw.obj+
$(OBJS)\d_zpoint.obj+
$(OBJS)\d_vars.obj+
$(OBJS)\d_surf.obj+
$(OBJS)\d_sprite.obj+
$(OBJS)\d_sky.obj+
$(OBJS)\d_scan.obj+
$(OBJS)\d_polyse.obj+
$(OBJS)\d_part.obj+
$(OBJS)\d_modech.obj+
$(OBJS)\d_init.obj+
$(OBJS)\d_fill.obj+
$(OBJS)\r_misc.obj+
$(OBJS)\worlda.obj+
$(OBJS)\d_varsa.obj+
$(OBJS)\sys_x86.obj+
$(OBJS)\surf8.obj+
$(OBJS)\surf16.obj+
$(OBJS)\snd_mixa.obj+
$(OBJS)\r_varsa.obj+
$(OBJS)\r_edgea.obj+
$(OBJS)\r_drawa.obj+
$(OBJS)\r_aliasa.obj+
$(OBJS)\r_aclipa.obj+
$(OBJS)\cl_math.obj+
$(OBJS)\math.obj+
$(OBJS)\d_draw.obj+
$(OBJS)\d_scana.obj+
$(OBJS)\d_polysa.obj+
$(OBJS)\d_parta.obj+
$(OBJS)\d_draw16.obj+
$(OBJS)\d_spr8.obj+
$(OBJS)\borland.obj+
$(OBJS)\buildnum.obj+
$(OBJS)\checksum.obj+
$(OBJS)\com.obj+
$(OBJS)\info.obj+
$(OBJS)\sizebuf.obj+
$(OBJS)\msg.obj+
$(OBJS)\va.obj+
$(OBJS)\qargs.obj+
$(OBJS)\quakefs.obj+
$(OBJS)\qendian.obj+
$(OBJS)\quakeio.obj+
$(OBJS)\net_udp.obj+
$(OBJS)\zone.obj+
$(OBJS)\pmovetst.obj+
$(OBJS)\pmove.obj+
$(OBJS)\net_com.obj+
$(OBJS)\net_chan.obj+
$(OBJS)\cmd.obj+
$(OBJS)\mdfour.obj+
$(OBJS)\cvar.obj+
$(OBJS)\crc.obj+
$(OBJS)\fnmatch.obj+
$(OBJS)\sys_win.obj+
$(OBJS)\snd_win.obj+
$(OBJS)\cd_sdl.obj+
$(OBJS)\in_sdl.obj+
$(OBJS)\cl_sys_sdl.obj+
$(OBJS)\cl_slist.obj+
$(OBJS)\mathlib.obj+
$(OBJS)\nonintel.obj+
$(OBJS)\menu.obj+
$(OBJS)\keys.obj+
$(OBJS)\console.obj+
$(OBJS)\wad.obj+
$(OBJS)\snd_mix.obj+
$(OBJS)\snd_mem.obj+
$(OBJS)\snd_dma.obj+
$(OBJS)\skin.obj+
$(OBJS)\cl_cam.obj+
$(OBJS)\cl_tent.obj+
$(OBJS)\cl_pred.obj+
$(OBJS)\cl_parse.obj+
$(OBJS)\cl_misc.obj+
$(OBJS)\cl_main.obj+
$(OBJS)\cl_input.obj+
$(OBJS)\cl_ents.obj+
$(OBJS)\cl_demo.obj+
$(OBJS)\cl_cvar.obj+
$(OBJS)\cl_cmd.obj+
$(OBJS)\sbar.obj
$<,$*
$(DIRECTXSDK)\lib\borland\dxguid.lib+
$(SDLSDK)\lib\sdl.lib+
$(CROOT)\LIB\import32.lib+
$(CROOT)\LIB\cw32.lib

|
$(OBJS)\snd_dma.obj :  $(QFROOT)\source\snd_dma.c
  $(BCC32) -P- -c  @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\snd_dma.c

|
$(OBJS)\hash.obj :  $(QFROOT)\source\hash.c
  $(BCC32) -P- -c  @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\hash.c

|
$(OBJS)\pcx.obj :  $(QFROOT)\source\pcx.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\pcx.c

|
$(OBJS)\vid.obj :  $(QFROOT)\source\vid.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\vid.c

|
$(OBJS)\joy_null.obj :  $(QFROOT)\source\joy_null.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\joy_null.c

|
$(OBJS)\locs.obj :  $(QFROOT)\source\locs.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\locs.c

|
$(OBJS)\model.obj :  $(QFROOT)\source\model.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\model.c

|
$(OBJS)\model_brush.obj :  $(QFROOT)\source\model_brush.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\model_brush.c

|
$(OBJS)\model_alias.obj :  $(QFROOT)\source\model_alias.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\model_alias.c

|
$(OBJS)\model_sprite.obj :  $(QFROOT)\source\model_sprite.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\model_sprite.c

|
$(OBJS)\sw_model_brush.obj :  $(QFROOT)\source\sw_model_brush.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sw_model_brush.c

|
$(OBJS)\sw_model_alias.obj :  $(QFROOT)\source\sw_model_alias.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sw_model_alias.c

|
$(OBJS)\sw_model_sprite.obj :  $(QFROOT)\source\sw_model_sprite.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sw_model_sprite.c

|
$(OBJS)\teamplay.obj :  $(QFROOT)\source\teamplay.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\teamplay.c

|
$(OBJS)\vid_sdl.obj :  $(QFROOT)\source\vid_sdl.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\vid_sdl.c
|

$(OBJS)\sdl_main.obj :  $(SDLSDK)\src\main\win32\sdl_main.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(SDLSDK)\src\main\win32\sdl_main.c
|

$(OBJS)\sw_skin.obj :  $(QFROOT)\source\sw_skin.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sw_skin.c
|

$(OBJS)\sw_view.obj :  $(QFROOT)\source\sw_view.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sw_view.c
|

$(OBJS)\r_view.obj :  $(QFROOT)\source\r_view.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_view.c
|

$(OBJS)\r_sprite.obj :  $(QFROOT)\source\r_sprite.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_sprite.c
|

$(OBJS)\r_part.obj :  $(QFROOT)\source\r_part.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_part.c
|

$(OBJS)\r_vars.obj :  $(QFROOT)\source\r_vars.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_vars.c
|

$(OBJS)\r_surf.obj :  $(QFROOT)\source\r_surf.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_surf.c
|

$(OBJS)\r_sky.obj :  $(QFROOT)\source\r_sky.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_sky.c
|

$(OBJS)\screen.obj :  $(QFROOT)\source\screen.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\screen.c
|

$(OBJS)\d_edge.obj :  $(QFROOT)\source\d_edge.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_edge.c
|

$(OBJS)\r_main.obj :  $(QFROOT)\source\r_main.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_main.c
|

$(OBJS)\r_light.obj :  $(QFROOT)\source\r_light.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_light.c
|

$(OBJS)\r_efrag.obj :  $(QFROOT)\source\r_efrag.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_efrag.c
|

$(OBJS)\r_edge.obj :  $(QFROOT)\source\r_edge.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_edge.c
|

$(OBJS)\r_draw.obj :  $(QFROOT)\source\r_draw.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_draw.c
|

$(OBJS)\r_bsp.obj :  $(QFROOT)\source\r_bsp.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_bsp.c
|

$(OBJS)\r_alias.obj :  $(QFROOT)\source\r_alias.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_alias.c
|

$(OBJS)\r_aclip.obj :  $(QFROOT)\source\r_aclip.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_aclip.c
|

$(OBJS)\draw.obj :  $(QFROOT)\source\draw.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\draw.c
|

$(OBJS)\d_zpoint.obj :  $(QFROOT)\source\d_zpoint.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_zpoint.c
|

$(OBJS)\d_vars.obj :  $(QFROOT)\source\d_vars.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_vars.c
|

$(OBJS)\d_surf.obj :  $(QFROOT)\source\d_surf.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_surf.c
|

$(OBJS)\d_sprite.obj :  $(QFROOT)\source\d_sprite.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_sprite.c
|

$(OBJS)\d_sky.obj :  $(QFROOT)\source\d_sky.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_sky.c
|

$(OBJS)\d_scan.obj :  $(QFROOT)\source\d_scan.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_scan.c
|

$(OBJS)\d_polyse.obj :  $(QFROOT)\source\d_polyse.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_polyse.c
|

$(OBJS)\d_part.obj :  $(QFROOT)\source\d_part.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_part.c
|

$(OBJS)\d_modech.obj :  $(QFROOT)\source\d_modech.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_modech.c
|

$(OBJS)\d_init.obj :  $(QFROOT)\source\d_init.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_init.c
|

$(OBJS)\d_fill.obj :  $(QFROOT)\source\d_fill.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\d_fill.c
|

$(OBJS)\r_misc.obj :  $(QFROOT)\source\r_misc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_misc.c
|

$(OBJS)\borland.obj :  $(QFROOT)\include\win32\bc\borland.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\include\win32\bc\borland.c
|

$(OBJS)\buildnum.obj :  $(QFROOT)\source\buildnum.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\buildnum.c
|

$(OBJS)\checksum.obj :  $(QFROOT)\source\checksum.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\checksum.c
|

$(OBJS)\com.obj :  $(QFROOT)\source\com.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\com.c
|

$(OBJS)\info.obj :  $(QFROOT)\source\info.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\info.c
|

$(OBJS)\sizebuf.obj :  $(QFROOT)\source\sizebuf.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sizebuf.c
|

$(OBJS)\msg.obj :  $(QFROOT)\source\msg.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\msg.c
|

$(OBJS)\va.obj :  $(QFROOT)\source\va.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\va.c
|

$(OBJS)\qargs.obj :  $(QFROOT)\source\qargs.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\qargs.c
|

$(OBJS)\quakefs.obj :  $(QFROOT)\source\quakefs.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\quakefs.c
|

$(OBJS)\qendian.obj :  $(QFROOT)\source\qendian.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\qendian.c
|

$(OBJS)\quakeio.obj :  $(QFROOT)\source\quakeio.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\quakeio.c
|

$(OBJS)\net_udp.obj :  $(QFROOT)\source\net_udp.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\net_udp.c
|

$(OBJS)\zone.obj :  $(QFROOT)\source\zone.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\zone.c
|

$(OBJS)\pmovetst.obj :  $(QFROOT)\source\pmovetst.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\pmovetst.c
|

$(OBJS)\pmove.obj :  $(QFROOT)\source\pmove.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\pmove.c
|

$(OBJS)\net_com.obj :  $(QFROOT)\source\net_com.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\net_com.c
|

$(OBJS)\net_chan.obj :  $(QFROOT)\source\net_chan.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\net_chan.c
|

$(OBJS)\cmd.obj :  $(QFROOT)\source\cmd.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cmd.c
|

$(OBJS)\mdfour.obj :  $(QFROOT)\source\mdfour.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\mdfour.c
|

$(OBJS)\cvar.obj :  $(QFROOT)\source\cvar.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cvar.c
|

$(OBJS)\crc.obj :  $(QFROOT)\source\crc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\crc.c
|

$(OBJS)\fnmatch.obj :  $(QFROOT)\source\fnmatch.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\fnmatch.c
|

$(OBJS)\sys_win.obj :  $(QFROOT)\source\sys_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sys_win.c
|

$(OBJS)\snd_win.obj :  $(QFROOT)\source\snd_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\snd_win.c
|

$(OBJS)\cd_sdl.obj :  $(QFROOT)\source\cd_sdl.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cd_sdl.c
|

$(OBJS)\in_sdl.obj :  $(QFROOT)\source\in_sdl.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\in_sdl.c
|

$(OBJS)\cl_sys_sdl.obj :  $(QFROOT)\source\cl_sys_sdl.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_sys_sdl.c
|

$(OBJS)\cl_slist.obj :  $(QFROOT)\source\cl_slist.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_slist.c
|

$(OBJS)\mathlib.obj :  $(QFROOT)\source\mathlib.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\mathlib.c
|

$(OBJS)\nonintel.obj :  $(QFROOT)\source\nonintel.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\nonintel.c
|

$(OBJS)\menu.obj :  $(QFROOT)\source\menu.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\menu.c
|

$(OBJS)\keys.obj :  $(QFROOT)\source\keys.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\keys.c
|

$(OBJS)\console.obj :  $(QFROOT)\source\console.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\console.c
|

$(OBJS)\wad.obj :  $(QFROOT)\source\wad.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\wad.c
|

$(OBJS)\snd_mix.obj :  $(QFROOT)\source\snd_mix.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\snd_mix.c
|

$(OBJS)\snd_mem.obj :  $(QFROOT)\source\snd_mem.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\snd_mem.c
|


$(OBJS)\skin.obj :  $(QFROOT)\source\skin.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\skin.c
|

$(OBJS)\cl_cam.obj :  $(QFROOT)\source\cl_cam.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_cam.c
|

$(OBJS)\cl_tent.obj :  $(QFROOT)\source\cl_tent.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_tent.c
|

$(OBJS)\cl_pred.obj :  $(QFROOT)\source\cl_pred.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_pred.c
|

$(OBJS)\cl_parse.obj :  $(QFROOT)\source\cl_parse.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_parse.c
|

$(OBJS)\cl_misc.obj :  $(QFROOT)\source\cl_misc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_misc.c
|

$(OBJS)\cl_main.obj :  $(QFROOT)\source\cl_main.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_main.c
|

$(OBJS)\cl_input.obj :  $(QFROOT)\source\cl_input.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_input.c
|

$(OBJS)\cl_ents.obj :  $(QFROOT)\source\cl_ents.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_ents.c
|

$(OBJS)\cl_demo.obj :  $(QFROOT)\source\cl_demo.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_demo.c
|

$(OBJS)\cl_cvar.obj :  $(QFROOT)\source\cl_cvar.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_cvar.c
|

$(OBJS)\cl_cmd.obj :  $(QFROOT)\source\cl_cmd.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cl_cmd.c
|

$(OBJS)\sbar.obj :  $(QFROOT)\source\sbar.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sbar.c
|

$(OBJS)\d_draw.obj :  $(QFROOT)\source\d_draw.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\d_draw$(EXT2) $(ASMOUT)\d_draw$(EXT1)
|

$(OBJS)\sys_x86.obj :  $(QFROOT)\source\sys_x86.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\sys_x86$(EXT2) $(ASMOUT)\sys_x86$(EXT1)
|

$(OBJS)\surf8.obj :  $(QFROOT)\source\surf8.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\surf8$(EXT2) $(ASMOUT)\surf8$(EXT1)
|

$(OBJS)\surf16.obj :  $(QFROOT)\source\surf16.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\surf16$(EXT2) $(ASMOUT)\surf16$(EXT1)
|

$(OBJS)\snd_mixa.obj :  $(QFROOT)\source\snd_mixa.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\snd_mixa$(EXT2) $(ASMOUT)\snd_mixa$(EXT1)
|

$(OBJS)\r_varsa.obj :  $(QFROOT)\source\r_varsa.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\r_varsa$(EXT2) $(ASMOUT)\r_varsa$(EXT1)
|

$(OBJS)\r_edgea.obj :  $(QFROOT)\source\r_edgea.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\r_edgea$(EXT2) $(ASMOUT)\r_edgea$(EXT1)
|

$(OBJS)\r_drawa.obj :  $(QFROOT)\source\r_drawa.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\r_drawa$(EXT2) $(ASMOUT)\r_drawa$(EXT1)
|

$(OBJS)\r_aliasa.obj :  $(QFROOT)\source\r_aliasa.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\r_aliasa$(EXT2) $(ASMOUT)\r_aliasa$(EXT1)
|

$(OBJS)\r_aclipa.obj :  $(QFROOT)\source\r_aclipa.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\r_aclipa$(EXT2) $(ASMOUT)\r_aclipa$(EXT1)
|

$(OBJS)\math.obj :  $(QFROOT)\source\math.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\math$(EXT2) $(ASMOUT)\math$(EXT1)
|

$(OBJS)\cl_math.obj :  $(QFROOT)\source\cl_math.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\cl_math$(EXT2) $(ASMOUT)\cl_math$(EXT1)
|

$(OBJS)\d_varsa.obj :  $(QFROOT)\source\d_varsa.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\d_varsa$(EXT2) $(ASMOUT)\d_varsa$(EXT1)
|

$(OBJS)\d_spr8.obj :  $(QFROOT)\source\d_spr8.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\d_spr8$(EXT2) $(ASMOUT)\d_spr8$(EXT1)
|

$(OBJS)\d_scana.obj :  $(QFROOT)\source\d_scana.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\d_scana$(EXT2) $(ASMOUT)\d_scana$(EXT1)
|

$(OBJS)\d_polysa.obj :  $(QFROOT)\source\d_polysa.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\d_polysa$(EXT2) $(ASMOUT)\d_polysa$(EXT1)
|

$(OBJS)\d_parta.obj :  $(QFROOT)\source\d_parta.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\d_parta$(EXT2) $(ASMOUT)\d_parta$(EXT1)
|

$(OBJS)\d_draw16.obj :  $(QFROOT)\source\d_draw16.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\d_draw16$(EXT2) $(ASMOUT)\d_draw16$(EXT1)
|

$(OBJS)\worlda.obj :  $(QFROOT)\source\worlda.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\worlda$(EXT2) $(ASMOUT)\worlda$(EXT1)
|

