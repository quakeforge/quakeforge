Requirements
=============
- Visual C++ 2005 SP1
- Windows Vista SDK (previous RC compilers won't recognize the
  high-resolution Vista icon)
- DirectX SDK (for DirectInput)
- SDL (required for sdl, sdl32, and sgl builds)
- bison/flex (required for qfcc, needs to be in your compiler path)

Optional
=========
- zlib (#undef HAVE_ZLIB from vc2005/include/config.h if you don't want
  this).  Expects zlib.lib and zlib.dll for Debug/Release builds, and
  libzlib.lib for Release (static) build.
- libcurl (#undef HAVE_LIBCURL from vc2005/include/config.h if you don't
  want this).  Expects curl.lib and curl.dll for Debug/Release builds,
  and libcurl.lib for Release (static) build.

Notes
======
By default, qfcc is configured to use the Boost Wave preprocessor.  You
can get this from http://www.boost.org, or change the CPP_NAME #define in
vc2005/include/config.h to whatever preprocessor you want.  If you have
GCC, you can simply replace "wave --c99" with "gcc" in the define and it
should work.

clean.ps1 is a Windows Powershell script that cleans up any build files.
