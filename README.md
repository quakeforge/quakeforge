# QuakeFor

QuakeForge is descended from the original Quake engine as released by Id
Software in December 1999, and can be used to play original Quake and
QuakeWorld games and mods (including many modern mods). While this will
always be the case, development continues.

However, QuakeForge is not just a Quake engine, but includes a
collection of tools for creating Quake mods, and is progressing towards
being a more general game engine.

## Quake and QuakeWorld

Support for Quake and QuakeWorld is split into two program sets: nq for
Quake and qw-client for QuakeWorld, with the target system as an
additional suffix: -x11 For the X Window system (Linux, BSD, etc), -win
for MS Windows (plus others that are not currently maintained).

Both nq and qw-client support multiple renderers: 8-bit software, 32-bit
software, OpenGL 2, EGL (mostly, one non-EGL function is used), and
Vulkan (very WIP), all within the one executable.

Dedicated servers for both Quake (nq-server) and QuakeWorld (qw-server)
are included, as well as a master server for QuakeWorld (qw-master).

## Tool

QuakeForge includes several tools for working with Quake data:
- bsp2image produces wireframe images from Quake maps (bsp files)
- io_mesh_qfmdl for importing and exporting Quake mdl files to/from
  Blender
- io_qfmap for Quake map source files (WIP Blender addon)
- pak create, list and extract Quake pak files. There's also zpak which
  can be used to compress the contents of pak files using gzip
  (QuakeForge has transparent support for gzip compressed files)
- qfbsp for compiling map files to bsp files, includes support for
  vis clusters, and can be used to extract data and information from bsp
  files.
- qfcc is QuakeForge's version of qcc, but is significantly more
  advanced, with support for standard C syntax, including most C types,
  as well as Objective-C object oriented programming (Ruamoko). Mmost of
  the advanced features require the QuakeForge engine, but qfcc can
  produce progs files compatible with the original Quake engine with
  limited support for some of the advanced featuers (C syntax, reduced
  global usage, some additional operators (eg, better bit operators,
  remainder (%)). Includes qfprogs for inspecting progs files.
- qflight creates lightmaps for Quake maps
- qfvis for compiling PVS data for Quake maps. One of the faster
  implementations available.
- Plus a few others in various stages of usefulness: qflmp, qfmodelgen,
  qfspritegen, wad, qfwavinfo

## Building

For now, please refer to INSTALL for information on building on Linux.
Building for windows is done by cross-compiling using MXE. There are
scripts in tools/mingw and tools/mingw64 that automate the process of
configuring and building both the tools run on the build-host and the
windows targets.
