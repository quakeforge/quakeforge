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
# IDE macros
#


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

# Where you want to place those .obj files
#OBJS = $(QFROOT)\TARGETS\GLQW_CLIENT
OBJS = $(QFROOT)\SOURCE

# ... and final exe
#EXE = $(QFROOT)\TARGETS
EXE = $(QFROOT)

# Path to your Direct-X libraries and includes
DIRECTXSDK=D:\project\dx7sdk
# Path to your SDL SDK
SDLSDK=D:\project\SDL-1.1.6
# Path to ZLIB source code
ZLIB=D:\PROJECT\ZLIB

# end of system dependant stuffs

SYSLIBS = $(CROOT)\LIB
MISCLIBS = $(DIRECTXSDK)\lib\borland;
LIBS=$(SYSLIBS);$(MISCLIBS)

SYSINCLUDE = $(CROOT)\INCLUDE
QFINCLUDES = $(QFROOT)\INCLUDE\WIN32\BC;$(QFROOT)\INCLUDE\WIN32;$(QFROOT)\INCLUDE
MISCINCLUDES = $(DIRECTXSDK)\include;$(SDLSDK)\include;$(ZLIB)

INCLUDES = $(QFINCLUDES);$(SYSINCLUDE);$(MISCINCLUDES)

DEFINES=_WINDOWS=1;_WIN32=1;WINDOWS=1;WIN32=1;HAVE_CONFIG_H=1;HAVE_FNMATCH_H=1;USE_INTEL_ASM=1

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
   $(OBJS)\qfgl_ext.obj\
   $(OBJS)\vid_common_gl.obj\
   $(OBJS)\tga.obj\
   $(OBJS)\fractalnoise.obj\
   $(OBJS)\gl_dyn_textures.obj\
   $(OBJS)\gl_sky.obj\
   $(OBJS)\gl_sky_clip.obj\
   $(OBJS)\gl_dyn_fires.obj\
   $(OBJS)\gl_dyn_part.obj\
   $(OBJS)\vid.obj\
   $(OBJS)\joy_null.obj\
   $(OBJS)\locs.obj\
   $(ZLIB)\zlib.lib\
   $(DIRECTXSDK)\lib\borland\dxguid.lib\
   $(OBJS)\model.obj\
   $(OBJS)\model_brush.obj\
   $(OBJS)\model_alias.obj\
   $(OBJS)\model_sprite.obj\
   $(OBJS)\gl_model_alias.obj\
   $(OBJS)\gl_model_fullbright.obj\
   $(OBJS)\gl_model_brush.obj\
   $(OBJS)\gl_model_sprite.obj\
   $(OBJS)\teamplay.obj\
   $(OBJS)\sdl_main.obj\
   $(OBJS)\r_view.obj\
   $(OBJS)\gl_view.obj\
   $(OBJS)\vid_sgl.obj\
   $(QFROOT)\opengl32.lib\
   $(SDLSDK)\lib\sdl.lib\
   $(OBJS)\gl_draw.obj\
   $(OBJS)\gl_skin.obj\
   $(OBJS)\gl_screen.obj\
   $(OBJS)\gl_rsurf.obj\
   $(OBJS)\gl_rmisc.obj\
   $(OBJS)\gl_rmain.obj\
   $(OBJS)\gl_rlight.obj\
   $(OBJS)\r_efrag.obj\
   $(OBJS)\gl_ngraph.obj\
   $(OBJS)\gl_mesh.obj\
   $(OBJS)\gl_warp.obj\
   $(OBJS)\worlda.obj\
   $(OBJS)\sys_x86.obj\
   $(OBJS)\snd_mixa.obj\
   $(OBJS)\math.obj\
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

$(EXE)\qf-client-sgl.exe : $(DEPEND)
  $(TLINK32) @&&|
 /v $(LINKOPTS) +
$(CROOT)\LIB\c0w32.obj+
$(OBJS)\hash.obj+
$(OBJS)\pcx.obj+
$(OBJS)\qfgl_ext.obj+
$(OBJS)\vid_common_gl.obj+
$(OBJS)\tga.obj+
$(OBJS)\fractalnoise.obj+
$(OBJS)\gl_dyn_textures.obj+
$(OBJS)\gl_sky.obj+
$(OBJS)\gl_sky_clip.obj+
$(OBJS)\gl_dyn_fires.obj+
$(OBJS)\gl_dyn_part.obj+
$(OBJS)\vid.obj+
$(OBJS)\joy_null.obj+
$(OBJS)\locs.obj+
$(ZLIB)\zlib.lib+
$(OBJS)\model.obj+
$(OBJS)\model_brush.obj+
$(OBJS)\model_alias.obj+
$(OBJS)\model_sprite.obj+
$(OBJS)\gl_model_alias.obj+
$(OBJS)\gl_model_fullbright.obj+
$(OBJS)\gl_model_brush.obj+
$(OBJS)\gl_model_sprite.obj+
$(OBJS)\teamplay.obj+
$(OBJS)\sdl_main.obj+
$(OBJS)\r_view.obj+
$(OBJS)\gl_view.obj+
$(OBJS)\vid_sgl.obj+
$(OBJS)\gl_draw.obj+
$(OBJS)\gl_skin.obj+
$(OBJS)\gl_screen.obj+
$(OBJS)\gl_rsurf.obj+
$(OBJS)\gl_rmisc.obj+
$(OBJS)\gl_rmain.obj+
$(OBJS)\gl_rlight.obj+
$(OBJS)\r_efrag.obj+
$(OBJS)\gl_ngraph.obj+
$(OBJS)\gl_mesh.obj+
$(OBJS)\gl_warp.obj+
$(OBJS)\worlda.obj+
$(OBJS)\sys_x86.obj+
$(OBJS)\snd_mixa.obj+
$(OBJS)\math.obj+
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
$(QFROOT)\opengl32.lib+
$(DIRECTXSDK)\lib\borland\dxguid.lib+
$(SDLSDK)\lib\sdl.lib+
$(CROOT)\LIB\import32.lib+
$(CROOT)\LIB\cw32.lib

|
$(OBJS)\pcx.obj :  $(QFROOT)\source\pcx.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\pcx.c

|
$(OBJS)\hash.obj :  $(QFROOT)\source\hash.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\hash.c

|
$(OBJS)\qfgl_ext.obj :  $(QFROOT)\source\qfgl_ext.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\qfgl_ext.c

|
$(OBJS)\vid_common_gl.obj :  $(QFROOT)\source\vid_common_gl.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\vid_common_gl.c

|
$(OBJS)\tga.obj :  $(QFROOT)\source\tga.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\tga.c

|
$(OBJS)\fractalnoise.obj :  $(QFROOT)\source\fractalnoise.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\fractalnoise.c

|
$(OBJS)\gl_dyn_textures.obj :  $(QFROOT)\source\gl_dyn_textures.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_dyn_textures.c

|
$(OBJS)\gl_sky.obj :  $(QFROOT)\source\gl_sky.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_sky.c

|
$(OBJS)\gl_sky_clip.obj :  $(QFROOT)\source\gl_sky_clip.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_sky_clip.c

|
$(OBJS)\gl_dyn_fires.obj :  $(QFROOT)\source\gl_dyn_fires.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_dyn_fires.c

|
$(OBJS)\gl_dyn_part.obj :  $(QFROOT)\source\gl_dyn_part.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_dyn_part.c

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
$(OBJS)\gl_model_brush.obj :  $(QFROOT)\source\gl_model_brush.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_model_brush.c

|
$(OBJS)\gl_model_alias.obj :  $(QFROOT)\source\gl_model_alias.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_model_alias.c

|
$(OBJS)\gl_model_fullbright.obj :  $(QFROOT)\source\gl_model_fullbright.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_model_fullbright.c

|
$(OBJS)\gl_model_sprite.obj :  $(QFROOT)\source\gl_model_sprite.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_model_sprite.c


|
$(OBJS)\teamplay.obj :  $(QFROOT)\source\teamplay.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\teamplay.c
|

$(OBJS)\sdl_main.obj :  $(SDLSDK)\src\main\win32\sdl_main.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(SDLSDK)\src\main\win32\sdl_main.c
|

$(OBJS)\r_view.obj :  $(QFROOT)\source\r_view.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_view.c
|

$(OBJS)\gl_view.obj :  $(QFROOT)\source\gl_view.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_view.c
|

$(OBJS)\vid_sgl.obj :  $(QFROOT)\source\vid_sgl.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\vid_sgl.c
|

$(OBJS)\gl_draw.obj :  $(QFROOT)\source\gl_draw.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_draw.c
|

$(OBJS)\gl_skin.obj :  $(QFROOT)\source\gl_skin.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_skin.c
|

$(OBJS)\gl_screen.obj :  $(QFROOT)\source\gl_screen.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_screen.c
|

$(OBJS)\gl_rsurf.obj :  $(QFROOT)\source\gl_rsurf.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_rsurf.c
|

$(OBJS)\gl_rmisc.obj :  $(QFROOT)\source\gl_rmisc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_rmisc.c
|

$(OBJS)\gl_rmain.obj :  $(QFROOT)\source\gl_rmain.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_rmain.c
|

$(OBJS)\gl_rlight.obj :  $(QFROOT)\source\gl_rlight.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_rlight.c
|

$(OBJS)\r_efrag.obj :  $(QFROOT)\source\r_efrag.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\r_efrag.c
|

$(OBJS)\gl_ngraph.obj :  $(QFROOT)\source\gl_ngraph.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_ngraph.c
|

$(OBJS)\gl_mesh.obj :  $(QFROOT)\source\gl_mesh.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_mesh.c
|

$(OBJS)\gl_warp.obj :  $(QFROOT)\source\gl_warp.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\gl_warp.c
|

$(OBJS)\borland.obj :  $(QFROOT)\INCLUDE\WIN32\BC\borland.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\INCLUDE\WIN32\BC\borland.c
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

$(OBJS)\snd_dma.obj :  $(QFROOT)\source\snd_dma.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\snd_dma.c
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

$(OBJS)\worlda.obj :  $(QFROOT)\source\worlda.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\worlda$(EXT2) $(ASMOUT)\worlda$(EXT1)
|

$(OBJS)\math.obj :  $(QFROOT)\source\math.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\math$(EXT2) $(ASMOUT)\math$(EXT1)
|

$(OBJS)\snd_mixa.obj :  $(QFROOT)\source\snd_mixa.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\snd_mixa$(EXT2) $(ASMOUT)\snd_mixa$(EXT1)
|

$(OBJS)\sys_x86.obj :  $(QFROOT)\source\sys_x86.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\sys_x86$(EXT2) $(ASMOUT)\sys_x86$(EXT1)
|

