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
#CROOT = D:\BORLAND\BCC55
CROOT = D:\progra\BCC55
# For 5.02
#CROOT = D:\BC5

# Where you want to place those .obj files
#OBJS = $(QFROOT)\TARGETS\SERVER
OBJS = $(QFROOT)\SOURCE

# ... and final exe
#EXE = $(QFROOT)\TARGETS
EXE = $(QFROOT)

# Path to ZLIB source code
ZLIB=D:\PROJECT\ZLIB

# end of system dependant stuffs

SYSLIBS = $(CROOT)\LIB
MISCLIBS = 
LIBS=$(SYSLIBS);$(MISCLIBS)

SYSINCLUDE = $(CROOT)\INCLUDE
QFINCLUDES = $(QFROOT)\INCLUDE\WIN32\BC;$(QFROOT)\INCLUDE\WIN32;$(QFROOT)\INCLUDE
MISCINCLUDES = $(ZLIB)

INCLUDES = $(QFINCLUDES);$(SYSINCLUDE);$(MISCINCLUDES)

DEFINES=WIN32=1;_WIN32=1;WINDOWS=1;_WINDOWS=1;HAVE_CONFIG_H=1;HAVE_FNMATCH_H=1;USE_INTEL_ASM=1

DEBUGOPTS = -y -k- -r -v

# for basic optimizations for 386
OPT = -O-d -3 -Oc -Oi -Ov -a4 -OS
# for Pentium
#OPT = -O-d -5 -Oc -Oi -Ov -a4 -OS
# for Pentium Pro and higher
#OPT = -O-d -6 -Oc -Oi -Ov -a4 -OS
# Testing purposes
#OPT = -O-d -5 -Oc -Oi -Ov -a4 -Og -Oc -Ol -Ob -Oe -Om -Op

WARNS =  -w-pro- -w-aus- -w-csu- -w-par- -w-pck-

COMPOPTS = $(DEBUGOPTS) $(OPT) $(WARNS) -WC -He- -f -ff -fp- -W

LINKOPTS = -w-dup -Tpe -ap -w-dpl -c -x -v- -L$(LIBS)

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

#
# Dependency List
#
DEPEND = \
   $(OBJS)\hash.obj\
   $(OBJS)\sv_progs.obj\
   $(OBJS)\sys_x86.obj\
   $(OBJS)\worlda.obj\
   $(OBJS)\ver_check.obj\
   $(ZLIB)\zlib.lib\
   $(OBJS)\model.obj\
   $(OBJS)\model_brush.obj\
   $(OBJS)\pr_offs.obj\
   $(OBJS)\buildnum.obj\
   $(OBJS)\info.obj\
   $(OBJS)\link.obj\
   $(OBJS)\checksum.obj\
   $(OBJS)\com.obj\
   $(OBJS)\sizebuf.obj\
   $(OBJS)\msg.obj\
   $(OBJS)\fnmatch.obj\
   $(OBJS)\quakefs.obj\
   $(OBJS)\quakeio.obj\
   $(OBJS)\va.obj\
   $(OBJS)\qendian.obj\
   $(OBJS)\qargs.obj\
   $(OBJS)\net_udp.obj\
   $(OBJS)\borland.obj\
   $(OBJS)\sv_sys_win.obj\
   $(OBJS)\zone.obj\
   $(OBJS)\pmovetst.obj\
   $(OBJS)\pmove.obj\
   $(OBJS)\net_com.obj\
   $(OBJS)\net_chan.obj\
   $(OBJS)\mdfour.obj\
   $(OBJS)\math.obj\
   $(OBJS)\mathlib.obj\
   $(OBJS)\cvar.obj\
   $(OBJS)\crc.obj\
   $(OBJS)\cmd.obj\
   $(OBJS)\world.obj\
   $(OBJS)\sys_win.obj\
   $(OBJS)\sv_user.obj\
   $(OBJS)\sv_send.obj\
   $(OBJS)\sv_phys.obj\
   $(OBJS)\sv_nchan.obj\
   $(OBJS)\sv_move.obj\
   $(OBJS)\sv_model.obj\
   $(OBJS)\sv_misc.obj\
   $(OBJS)\sv_main.obj\
   $(OBJS)\sv_init.obj\
   $(OBJS)\sv_ents.obj\
   $(OBJS)\sv_cvar.obj\
   $(OBJS)\sv_pr_cmds.obj\
   $(OBJS)\pr_exec.obj\
   $(OBJS)\pr_edict.obj\
   $(OBJS)\sv_ccmds.obj

$(EXE)\qf-server.exe : $(DEPEND)
  $(TLINK32) @&&|
 /v $(LINKOPTS) +
$(CROOT)\LIB\c0x32.obj+
$(OBJS)\hash.obj+
$(OBJS)\sv_progs.obj+
$(OBJS)\sys_x86.obj\
$(OBJS)\worlda.obj+
$(OBJS)\ver_check.obj+
$(ZLIB)\zlib.lib+
$(OBJS)\model.obj+
$(OBJS)\model_brush.obj+
$(OBJS)\pr_offs.obj+
$(OBJS)\buildnum.obj+
$(OBJS)\info.obj+
$(OBJS)\link.obj+
$(OBJS)\checksum.obj+
$(OBJS)\com.obj+
$(OBJS)\sizebuf.obj+
$(OBJS)\msg.obj+
$(OBJS)\fnmatch.obj+
$(OBJS)\quakefs.obj+
$(OBJS)\quakeio.obj+
$(OBJS)\va.obj+
$(OBJS)\qendian.obj+
$(OBJS)\qargs.obj+
$(OBJS)\net_udp.obj+
$(OBJS)\borland.obj+
$(OBJS)\sv_sys_win.obj+
$(OBJS)\zone.obj+
$(OBJS)\pmovetst.obj+
$(OBJS)\pmove.obj+
$(OBJS)\net_com.obj+
$(OBJS)\net_chan.obj+
$(OBJS)\mdfour.obj+
$(OBJS)\math.obj+
$(OBJS)\mathlib.obj+
$(OBJS)\cvar.obj+
$(OBJS)\crc.obj+
$(OBJS)\cmd.obj+
$(OBJS)\world.obj+
$(OBJS)\sys_win.obj+
$(OBJS)\sv_user.obj+
$(OBJS)\sv_send.obj+
$(OBJS)\sv_phys.obj+
$(OBJS)\sv_nchan.obj+
$(OBJS)\sv_move.obj+
$(OBJS)\sv_model.obj+
$(OBJS)\sv_misc.obj+
$(OBJS)\sv_main.obj+
$(OBJS)\sv_init.obj+
$(OBJS)\sv_ents.obj+
$(OBJS)\sv_cvar.obj+
$(OBJS)\sv_pr_cmds.obj+
$(OBJS)\pr_exec.obj+
$(OBJS)\pr_edict.obj+
$(OBJS)\sv_ccmds.obj
$<,$*
$(CROOT)\LIB\import32.lib+
$(CROOT)\LIB\cw32.lib
#$(CROOT)\LIB\cw32mt.lib

|
$(OBJS)\hash.obj :  $(QFROOT)\source\hash.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\hash.c

|
$(OBJS)\ver_check.obj :  $(QFROOT)\source\ver_check.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\ver_check.c

|
$(OBJS)\sv_progs.obj :  $(QFROOT)\source\sv_progs.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_progs.c

|
$(OBJS)\model.obj :  $(QFROOT)\source\model.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\model.c

|
$(OBJS)\model_brush.obj :  $(QFROOT)\source\model_brush.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\model_brush.c

|
$(OBJS)\pr_offs.obj :  $(QFROOT)\source\pr_offs.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\pr_offs.c

|
$(OBJS)\buildnum.obj :  $(QFROOT)\source\buildnum.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\buildnum.c
|

$(OBJS)\info.obj :  $(QFROOT)\source\info.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\info.c
|

$(OBJS)\link.obj :  $(QFROOT)\source\link.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\link.c
|

$(OBJS)\checksum.obj :  $(QFROOT)\source\checksum.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\checksum.c
|

$(OBJS)\com.obj :  $(QFROOT)\source\com.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\com.c
|

$(OBJS)\sizebuf.obj :  $(QFROOT)\source\sizebuf.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sizebuf.c
|

$(OBJS)\msg.obj :  $(QFROOT)\source\msg.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\msg.c
|

$(OBJS)\fnmatch.obj :  $(QFROOT)\source\fnmatch.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\fnmatch.c
|

$(OBJS)\quakefs.obj :  $(QFROOT)\source\quakefs.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\quakefs.c
|

$(OBJS)\quakeio.obj :  $(QFROOT)\source\quakeio.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\quakeio.c
|

$(OBJS)\va.obj :  $(QFROOT)\source\va.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\va.c
|

$(OBJS)\qendian.obj :  $(QFROOT)\source\qendian.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\qendian.c
|

$(OBJS)\qargs.obj :  $(QFROOT)\source\qargs.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\qargs.c
|

$(OBJS)\net_udp.obj :  $(QFROOT)\source\net_udp.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\net_udp.c
|

$(OBJS)\borland.obj :  $(QFROOT)\include\win32\bc\borland.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\include\win32\bc\borland.c
|

$(OBJS)\sv_sys_win.obj :  $(QFROOT)\source\sv_sys_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_sys_win.c
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

$(OBJS)\mdfour.obj :  $(QFROOT)\source\mdfour.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\mdfour.c
|

$(OBJS)\mathlib.obj :  $(QFROOT)\source\mathlib.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\mathlib.c
|

$(OBJS)\cvar.obj :  $(QFROOT)\source\cvar.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cvar.c
|

$(OBJS)\crc.obj :  $(QFROOT)\source\crc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\crc.c
|

$(OBJS)\cmd.obj :  $(QFROOT)\source\cmd.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\cmd.c
|

$(OBJS)\world.obj :  $(QFROOT)\source\world.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\world.c
|

$(OBJS)\sys_win.obj :  $(QFROOT)\source\sys_win.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sys_win.c
|

$(OBJS)\sv_user.obj :  $(QFROOT)\source\sv_user.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_user.c
|

$(OBJS)\sv_send.obj :  $(QFROOT)\source\sv_send.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_send.c
|

$(OBJS)\sv_phys.obj :  $(QFROOT)\source\sv_phys.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_phys.c
|

$(OBJS)\sv_nchan.obj :  $(QFROOT)\source\sv_nchan.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_nchan.c
|

$(OBJS)\sv_move.obj :  $(QFROOT)\source\sv_move.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_move.c
|

$(OBJS)\sv_model.obj :  $(QFROOT)\source\sv_model.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_model.c
|

$(OBJS)\sv_misc.obj :  $(QFROOT)\source\sv_misc.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_misc.c
|

$(OBJS)\sv_main.obj :  $(QFROOT)\source\sv_main.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_main.c
|

$(OBJS)\sv_init.obj :  $(QFROOT)\source\sv_init.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_init.c
|

$(OBJS)\sv_ents.obj :  $(QFROOT)\source\sv_ents.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_ents.c
|

$(OBJS)\sv_cvar.obj :  $(QFROOT)\source\sv_cvar.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_cvar.c
|

$(OBJS)\sv_pr_cmds.obj :  $(QFROOT)\source\sv_pr_cmds.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_pr_cmds.c
|

$(OBJS)\pr_exec.obj :  $(QFROOT)\source\pr_exec.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\pr_exec.c
|

$(OBJS)\pr_edict.obj :  $(QFROOT)\source\pr_edict.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\pr_edict.c
|

$(OBJS)\sv_ccmds.obj :  $(QFROOT)\source\sv_ccmds.c
  $(BCC32) -P- -c @&&|
 $(COMPOPTS) -I$(INCLUDES) -D$(DEFINES) -o$@ $(QFROOT)\source\sv_ccmds.c
|

$(OBJS)\math.obj :  $(QFROOT)\source\math.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\math$(EXT2) $(ASMOUT)\math$(EXT1)
|

$(OBJS)\worlda.obj :  $(QFROOT)\source\worlda.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\worlda$(EXT2) $(ASMOUT)\worlda$(EXT1)
|

$(OBJS)\sys_x86.obj :  $(QFROOT)\source\sys_x86.asm
  $(ASSEMBLER) @&&|
 $(ASMOPTS) $(ASMIN)\sys_x86$(EXT2) $(ASMOUT)\sys_x86$(EXT1)
|

