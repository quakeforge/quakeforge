#! /usr/bin/env python
from os import system
import sys

version = ""
prefix = "qf-win32"
dir = prefix

if len (sys.argv) >= 2:
	version = "-" + sys.argv[1]
if len (sys.argv) >= 3:
	prefix = sys.argv[2]
if len (sys.argv) >= 4:
	dir = sys.argv[3]

if dir and dir[-1] != '/':
	dir += '/'

server = [
	dir,
	dir + "bin",
	dir + "bin/hw-master.exe",
	dir + "bin/nq-server.exe",
	dir + "bin/qw-master.exe",
	dir + "bin/qw-server.exe",
]

client_wgl = [
	dir,
	dir + "bin",
	dir + "bin/nq-wgl.exe",
	dir + "bin/qw-client-wgl.exe",
	dir + "menu.dat.gz",
]

client_sgl = [
	dir,
	dir + "bin",
	dir + "bin/nq-sgl.exe",
	dir + "bin/qw-client-sgl.exe",
	dir + "menu.dat.gz",
]

client_sdl = [
	dir,
	dir + "bin",
	dir + "bin/nq-sdl.exe",
	dir + "bin/qw-client-sdl.exe",
	dir + "menu.dat.gz",
]

client_sdl32 = [
	dir,
	dir + "bin",
	dir + "bin/nq-sdl32.exe",
	dir + "bin/qw-client-sdl32.exe",
	dir + "menu.dat.gz",
]

tools = [
	dir,
	dir + "bin",
	dir + "bin/bsp2img.exe",
	dir + "bin/pak.exe",
	dir + "bin/qfbsp.exe",
	dir + "bin/qfcc.exe",
	dir + "bin/qflight.exe",
	dir + "bin/qfmodelgen.exe",
	dir + "bin/qfprogs.exe",
	dir + "bin/qfvis.exe",
	dir + "bin/qfwavinfo.exe",
	dir + "bin/wad.exe",
	dir + "bin/zpak",
]

devel = [
	dir + "include",
	dir + "lib",
]

print "zip -9 " + prefix + "-server" + version + ".zip " + " ".join (server)
system ("zip -9 " + prefix + "-server" + version + ".zip " + " ".join (server))
print "zip -9 " + prefix + "-client-wgl" + version + ".zip " + " ".join (client_wgl)
system ("zip -9 " + prefix + "-client-wgl" + version + ".zip " + " ".join (client_wgl))
print "zip -9 " + prefix + "-client-sgl" + version + ".zip " + " ".join (client_sgl)
system ("zip -9 " + prefix + "-client-sgl" + version + ".zip " + " ".join (client_sgl))
print "zip -9 " + prefix + "-client-sdl" + version + ".zip " + " ".join (client_sdl)
system ("zip -9 " + prefix + "-client-sdl" + version + ".zip " + " ".join (client_sdl))
print "zip -9 " + prefix + "-client-sdl32" + version + ".zip " + " ".join (client_sdl32)
system ("zip -9 " + prefix + "-client-sdl32" + version + ".zip " + " ".join (client_sdl32))
print "zip -9 " + prefix + "-tools" + version + ".zip " + " ".join (tools)
system ("zip -9 " + prefix + "-tools" + version + ".zip " + " ".join (tools))
if dir:
	print "zip -9 " + prefix + "-devel" + version + ".zip " + dir
	system ("zip -9 " + prefix + "-devel" + version + ".zip " + dir)
print "zip -r9 " + prefix + "-devel" + version + ".zip " + " ".join (devel)
system ("zip -r9 " + prefix + "-devel" + version + ".zip " + " ".join (devel))
