# Microsoft Developer Studio Project File - Name="qw_server" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=qw_server - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "qw_server.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "qw_server.mak" CFG="qw_server - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "qw_server - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "qw_server - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "qw_server - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /WX /GX /O2 /I "../include" /I "../include/win32/vc" /I "../include/win32" /I "../include/win32/resources" /D "_CONSOLE" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D USE_INTEL_ASM=1 /D HAVE_CONFIG_H=1 /YX /FD /c
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib zlib.lib /nologo /subsystem:console /machine:I386 /out:"Release/qf-server.exe"
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "qw_server - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "qw_server___Win32_Debug"
# PROP BASE Intermediate_Dir "qw_server___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /WX /Gm /GX /ZI /Od /I "../include" /I "../include/win32/vc" /I "../include/win32" /I "../include/win32/resources" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /D USE_INTEL_ASM=1 /D HAVE_CONFIG_H=1 /YX /FD /GZ /c
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib zlibd.lib /nologo /subsystem:console /debug /machine:I386 /out:"Debug/qf-server.exe" /pdbtype:sept
# SUBTRACT LINK32 /nodefaultlib

!ENDIF 

# Begin Target

# Name "qw_server - Win32 Release"
# Name "qw_server - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\buildnum.c
# End Source File
# Begin Source File

SOURCE=.\checksum.c
# End Source File
# Begin Source File

SOURCE=.\cmd.c
# End Source File
# Begin Source File

SOURCE=.\com.c
# End Source File
# Begin Source File

SOURCE=.\crc.c
# End Source File
# Begin Source File

SOURCE=.\cvar.c
# End Source File
# Begin Source File

SOURCE=.\dirent.c
# End Source File
# Begin Source File

SOURCE=.\fnmatch.c
# End Source File
# Begin Source File

SOURCE=.\gl_sky.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\gl_sky_clip.c
# PROP Exclude_From_Build 1
# End Source File
# Begin Source File

SOURCE=.\hash.c
# End Source File
# Begin Source File

SOURCE=.\info.c
# End Source File
# Begin Source File

SOURCE=.\link.c
# End Source File
# Begin Source File

SOURCE=.\locs.c
# End Source File
# Begin Source File

SOURCE=.\mathlib.c
# End Source File
# Begin Source File

SOURCE=.\mdfour.c
# End Source File
# Begin Source File

SOURCE=.\model.c
# End Source File
# Begin Source File

SOURCE=.\model_brush.c
# End Source File
# Begin Source File

SOURCE=.\msg.c
# End Source File
# Begin Source File

SOURCE=.\net_chan.c
# End Source File
# Begin Source File

SOURCE=.\net_com.c
# End Source File
# Begin Source File

SOURCE=.\net_udp.c
# End Source File
# Begin Source File

SOURCE=.\pmove.c
# End Source File
# Begin Source File

SOURCE=.\pmovetst.c
# End Source File
# Begin Source File

SOURCE=.\pr_edict.c
# End Source File
# Begin Source File

SOURCE=.\pr_exec.c
# End Source File
# Begin Source File

SOURCE=.\pr_offs.c
# End Source File
# Begin Source File

SOURCE=.\qargs.c
# End Source File
# Begin Source File

SOURCE=.\qendian.c
# End Source File
# Begin Source File

SOURCE=..\include\win32\resources\quakeforge.rc
# End Source File
# Begin Source File

SOURCE=.\quakefs.c
# End Source File
# Begin Source File

SOURCE=.\quakeio.c
# End Source File
# Begin Source File

SOURCE=.\sizebuf.c
# End Source File
# Begin Source File

SOURCE=.\sv_ccmds.c
# End Source File
# Begin Source File

SOURCE=.\sv_cvar.c
# End Source File
# Begin Source File

SOURCE=.\sv_ents.c
# End Source File
# Begin Source File

SOURCE=.\sv_init.c
# End Source File
# Begin Source File

SOURCE=.\sv_main.c
# End Source File
# Begin Source File

SOURCE=.\sv_misc.c
# End Source File
# Begin Source File

SOURCE=.\sv_model.c
# End Source File
# Begin Source File

SOURCE=.\sv_move.c
# End Source File
# Begin Source File

SOURCE=.\sv_nchan.c
# End Source File
# Begin Source File

SOURCE=.\sv_phys.c
# End Source File
# Begin Source File

SOURCE=.\sv_pr_cmds.c
# End Source File
# Begin Source File

SOURCE=.\sv_progs.c
# End Source File
# Begin Source File

SOURCE=.\sv_send.c
# End Source File
# Begin Source File

SOURCE=.\sv_sys_win.c
# End Source File
# Begin Source File

SOURCE=.\sv_user.c
# End Source File
# Begin Source File

SOURCE=.\sys_win.c
# End Source File
# Begin Source File

SOURCE=.\va.c
# End Source File
# Begin Source File

SOURCE=.\ver_check.c
# End Source File
# Begin Source File

SOURCE=.\world.c
# End Source File
# Begin Source File

SOURCE=.\zone.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Group "Asm Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=.\math.S

!IF  "$(CFG)" == "qw_server - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\math.S
InputName=math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_server - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\math.S
InputName=math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sys_x86.S

!IF  "$(CFG)" == "qw_server - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\sys_x86.S
InputName=sys_x86

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_server - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\sys_x86.S
InputName=sys_x86

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\worlda.S

!IF  "$(CFG)" == "qw_server - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\worlda.S
InputName=worlda

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_server - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\worlda.S
InputName=worlda

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
