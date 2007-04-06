Requirements
=============
- Visual C++ 2005 SP1
- Windows Vista SDK (previous RC compilers won't recognize the
  high-resolution Vista icon)
- DirectX SDK (for DirectInput)

Optional
=========
- zlib (#undef HAVE_ZLIB from vc2005/include/config.h if you don't want
  this).  Expects zlib.lib and zlib.dll for Debug/Release builds, and
  libzlib.lib for Release (static) build.
- libcurl (#undef HAVE_LIBCURL from vc2005/include/config.h if you don't
  want this).  Expects curl.lib and curl.dll for Debug/Release builds,
  and libcurl.lib for Release (static) build.
