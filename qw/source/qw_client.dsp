# Microsoft Developer Studio Project File - Name="qw_client" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=qw_client - Win32 GLDebug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "qw_client.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "qw_client.mak" CFG="qw_client - Win32 GLDebug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "qw_client - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "qw_client - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE "qw_client - Win32 GLDebug" (based on "Win32 (x86) Application")
!MESSAGE "qw_client - Win32 GLRelease" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "qw_client - Win32 Release"

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
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /WX /GX /O2 /I "../include" /I "../include/win32/vc" /I "../include/win32" /I "../include/win32/resources" /D "_WINDOWS" /D "WIN32" /D "NDEBUG" /D "_MBCS" /D USE_INTEL_ASM=1 /D HAVE_CONFIG_H=1 /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 dxguid.lib mglfx.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib zlib.lib /nologo /subsystem:windows /machine:I386 /nodefaultlib:"libcmt" /out:"Release/qf-client-win.exe"
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "qw_client___Win32_Debug"
# PROP BASE Intermediate_Dir "qw_client___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /YX /FD /GZ /c
# ADD CPP /nologo /WX /Gm /GX /ZI /Od /I "../include" /I "../include/win32/vc" /I "../include/win32" /I "../include/win32/resources" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D USE_INTEL_ASM=1 /D HAVE_CONFIG_H=1 /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 dxguid.lib mglfx.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib zlibd.lib /nologo /subsystem:windows /debug /machine:I386 /nodefaultlib:"libcmt" /out:"Debug/qf-client-win.exe" /pdbtype:sept
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "qw_client___Win32_GLDebug"
# PROP BASE Intermediate_Dir "qw_client___Win32_GLDebug"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /I "../scitech/include" /I "../include" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "USE_INTEL_ASM" /YX /FD /GZ /c
# ADD CPP /nologo /WX /Gm /GX /ZI /Od /I "../include" /I "../include/win32/vc" /I "../include/win32" /I "../include/win32/resources" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D USE_INTEL_ASM=1 /D HAVE_CONFIG_H=1 /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "_DEBUG"
# ADD RSC /l 0x419 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 dxguid.lib ..\scitech\lib\win32\vc\mgllt.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 comctl32.lib opengl32.lib glu32.lib dxguid.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib zlibd.lib /nologo /subsystem:windows /debug /machine:I386 /out:"Debug/qf-client-wgl.exe" /pdbtype:sept
# SUBTRACT LINK32 /nodefaultlib

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "qw_client___Win32_GLRelease"
# PROP BASE Intermediate_Dir "qw_client___Win32_GLRelease"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /I "../scitech/include" /I "../include" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "USE_INTEL_ASM" /YX /FD /c
# ADD CPP /nologo /WX /GX /O2 /I "../include" /I "../include/win32/vc" /I "../include/win32" /I "../include/win32/resources" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D USE_INTEL_ASM=1 /D HAVE_CONFIG_H=1 /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x419 /d "NDEBUG"
# ADD RSC /l 0x419 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 dxguid.lib ..\scitech\lib\win32\vc\mgllt.lib wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib winmm.lib /nologo /subsystem:windows /machine:I386
# ADD LINK32 libc.lib comctl32.lib opengl32.lib glu32.lib dxguid.lib wsock32.lib winmm.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib zlib.lib /nologo /subsystem:windows /machine:I386 /out:"Release/qf-client-wgl.exe"
# SUBTRACT LINK32 /nodefaultlib

!ENDIF 

# Begin Target

# Name "qw_client - Win32 Release"
# Name "qw_client - Win32 Debug"
# Name "qw_client - Win32 GLDebug"
# Name "qw_client - Win32 GLRelease"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=.\buildnum.c
# End Source File
# Begin Source File

SOURCE=.\cd_win.c
# End Source File
# Begin Source File

SOURCE=.\checksum.c
# End Source File
# Begin Source File

SOURCE=.\cl_cam.c
# End Source File
# Begin Source File

SOURCE=.\cl_cmd.c
# End Source File
# Begin Source File

SOURCE=.\cl_cvar.c
# End Source File
# Begin Source File

SOURCE=.\cl_demo.c
# End Source File
# Begin Source File

SOURCE=.\cl_ents.c
# End Source File
# Begin Source File

SOURCE=.\cl_input.c
# End Source File
# Begin Source File

SOURCE=.\cl_main.c
# End Source File
# Begin Source File

SOURCE=.\cl_misc.c
# End Source File
# Begin Source File

SOURCE=.\cl_parse.c
# End Source File
# Begin Source File

SOURCE=.\cl_pred.c
# End Source File
# Begin Source File

SOURCE=.\cl_slist.c
# End Source File
# Begin Source File

SOURCE=.\cl_sys_win.c
# End Source File
# Begin Source File

SOURCE=.\cl_tent.c
# End Source File
# Begin Source File

SOURCE=.\cmd.c
# End Source File
# Begin Source File

SOURCE=.\com.c
# End Source File
# Begin Source File

SOURCE=.\console.c
# End Source File
# Begin Source File

SOURCE=.\crc.c
# End Source File
# Begin Source File

SOURCE=.\cvar.c
# End Source File
# Begin Source File

SOURCE=.\d_edge.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_fill.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_init.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_modech.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_part.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_polyse.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_scan.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_sky.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_sprite.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_surf.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_vars.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_zpoint.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\dirent.c
# End Source File
# Begin Source File

SOURCE=.\draw.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\fnmatch.c
# End Source File
# Begin Source File

SOURCE=.\fractalnoise.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_draw.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_dyn_fires.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_dyn_part.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_dyn_textures.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_mesh.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_model_alias.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_model_brush.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_model_fullbright.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_model_sprite.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_ngraph.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_rlight.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_rmain.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_rmisc.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_rsurf.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_screen.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_skin.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_sky.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_sky_clip.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_view.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\gl_warp.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\hash.c
# End Source File
# Begin Source File

SOURCE=.\in_win.c
# End Source File
# Begin Source File

SOURCE=.\info.c
# End Source File
# Begin Source File

SOURCE=.\joy_win.c
# End Source File
# Begin Source File

SOURCE=.\keys.c
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

SOURCE=.\menu.c
# End Source File
# Begin Source File

SOURCE=.\model.c
# End Source File
# Begin Source File

SOURCE=.\model_alias.c
# End Source File
# Begin Source File

SOURCE=.\model_brush.c
# End Source File
# Begin Source File

SOURCE=.\model_sprite.c
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

SOURCE=.\pcx.c
# End Source File
# Begin Source File

SOURCE=.\pmove.c
# End Source File
# Begin Source File

SOURCE=.\pmovetst.c
# End Source File
# Begin Source File

SOURCE=.\qargs.c
# End Source File
# Begin Source File

SOURCE=.\qendian.c
# End Source File
# Begin Source File

SOURCE=.\qfgl_ext.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

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

SOURCE=.\r_aclip.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_alias.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_bsp.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_draw.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_edge.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_efrag.c
# End Source File
# Begin Source File

SOURCE=.\r_light.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_main.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_misc.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_part.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_sky.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_sprite.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_surf.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_vars.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_view.c
# End Source File
# Begin Source File

SOURCE=.\sbar.c
# End Source File
# Begin Source File

SOURCE=.\screen.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sizebuf.c
# End Source File
# Begin Source File

SOURCE=.\skin.c
# End Source File
# Begin Source File

SOURCE=.\snd_dma.c
# End Source File
# Begin Source File

SOURCE=.\snd_mem.c
# End Source File
# Begin Source File

SOURCE=.\snd_mix.c
# End Source File
# Begin Source File

SOURCE=.\snd_win.c
# End Source File
# Begin Source File

SOURCE=.\sw_model_alias.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sw_model_brush.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sw_model_sprite.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sw_skin.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sw_view.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sys_win.c
# End Source File
# Begin Source File

SOURCE=.\teamplay.c
# End Source File
# Begin Source File

SOURCE=.\tga.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\va.c
# End Source File
# Begin Source File

SOURCE=.\vid_mgl.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\vid_wgl.c

!IF  "$(CFG)" == "qw_client - Win32 Release"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\wad.c
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
# Begin Source File

SOURCE=..\include\win32\resources\icon1.ico
# End Source File
# End Group
# Begin Group "Asm Files"

# PROP Default_Filter "s"
# Begin Source File

SOURCE=.\cl_math.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\cl_math.S
InputName=cl_math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\cl_math.S
InputName=cl_math

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_draw.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\d_draw.S
InputName=d_draw

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_draw.S
InputName=d_draw

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_draw16.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\d_draw16.S
InputName=d_draw16

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_draw16.S
InputName=d_draw16

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_parta.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\d_parta.S
InputName=d_parta

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_parta.S
InputName=d_parta

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_polysa.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\d_polysa.S
InputName=d_polysa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_polysa.S
InputName=d_polysa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_scana.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\d_scana.S
InputName=d_scana

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_scana.S
InputName=d_scana

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_spr8.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\d_spr8.S
InputName=d_spr8

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_spr8.S
InputName=d_spr8

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\d_varsa.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\d_varsa.S
InputName=d_varsa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\d_varsa.S
InputName=d_varsa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\math.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

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

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

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

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

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

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

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

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_aclipa.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\r_aclipa.S
InputName=r_aclipa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\r_aclipa.S
InputName=r_aclipa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_aliasa.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\r_aliasa.S
InputName=r_aliasa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\r_aliasa.S
InputName=r_aliasa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_drawa.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\r_drawa.S
InputName=r_drawa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\r_drawa.S
InputName=r_drawa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_edgea.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\r_edgea.S
InputName=r_edgea

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\r_edgea.S
InputName=r_edgea

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\r_varsa.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\r_varsa.S
InputName=r_varsa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\r_varsa.S
InputName=r_varsa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\snd_mixa.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\snd_mixa.S
InputName=snd_mixa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\snd_mixa.S
InputName=snd_mixa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\snd_mixa.S
InputName=snd_mixa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# Begin Custom Build
OutDir=.\Release
InputPath=.\snd_mixa.S
InputName=snd_mixa

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\surf16.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\surf16.S
InputName=surf16

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\surf16.S
InputName=surf16

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\surf8.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

# Begin Custom Build
OutDir=.\Release
InputPath=.\surf8.S
InputName=surf8

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

# Begin Custom Build
OutDir=.\Debug
InputPath=.\surf8.S
InputName=surf8

"$(OUTDIR)\$(InputName).obj" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	cl /D "USE_INTEL_ASM=1" /D "HAVE_CONFIG_H=1" /I "..\source" /I "..\include" /I "..\include\win32\vc" /I "..\include\win32" /nologo /EP > $(OUTDIR)\$(InputName).spp $(InputPath) 
	..\tools\gas2masm\debug\gas2masm < $(OUTDIR)\$(InputName).spp > $(OUTDIR)\$(InputName).asm 
	ml /nologo /c /Cp /coff /Zi /H64 /Fo$(OUTDIR)\$(InputName).obj $(OUTDIR)\$(InputName).asm 
	del $(OUTDIR)\$(InputName).spp 
	
# End Custom Build

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

# PROP Exclude_From_Build 1

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

# PROP Exclude_From_Build 1

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\sys_x86.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

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

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

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

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

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

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

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

!ENDIF 

# End Source File
# Begin Source File

SOURCE=.\worlda.S

!IF  "$(CFG)" == "qw_client - Win32 Release"

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

!ELSEIF  "$(CFG)" == "qw_client - Win32 Debug"

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

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLDebug"

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

!ELSEIF  "$(CFG)" == "qw_client - Win32 GLRelease"

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

!ENDIF 

# End Source File
# End Group
# End Target
# End Project
