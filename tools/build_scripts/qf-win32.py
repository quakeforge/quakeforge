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
	dir + "bin/qtv.exe",
]

#client_wgl = [
#	dir,
#	dir + "bin",
#	dir + "bin/nq-wgl.exe",
#	dir + "bin/qw-client-wgl.exe",
#	dir + "QF",
#	dir + "QF/menu.dat.gz",
#	dir + "QF/menu.plist",
#]

client_sdl = [
	dir,
	dir + "bin",
	dir + "bin/nq-sdl.exe",
	dir + "bin/qw-client-sdl.exe",
	dir + "QF",
	dir + "QF/menu.dat.gz",
	dir + "QF/menu.plist",
]

tools = [
	dir,
	dir + "bin",
	dir + "bin/bsp2img.exe",
	dir + "bin/pak.exe",
	dir + "bin/qfbsp.exe",
	dir + "bin/qflight.exe",
	dir + "bin/qfmodelgen.exe",
	dir + "bin/qfvis.exe",
	dir + "bin/qfwavinfo.exe",
	dir + "bin/wad.exe",
	dir + "bin/zpak",
]

qfcc = [
	dir,
	dir + "bin",
	dir + "bin/qfcc.exe",
	dir + "bin/qfprogs.exe",
	dir + "bin/qfpreqcc",
	dir + "qfcc.1",
	dir + "pkgconfig/qfcc.pc",
]
qfcc_r = [
	dir + "share/qfcc",
]

devel = [
	dir,
	dir + "pkgconfig/quakeforge.pc",
]

devel_r = [
	dir + "include",
	dir + "lib",
]

io_mesh_qfmdl = [
	dir,
]

io_mesh_qfmdl_r = [
	dir + "io_mesh_qfmdl",
]

packages = [
	(prefix + "-" + "server", server),
	#(prefix + "-" + "client-wgl", client_wgl),
	(prefix + "-" + "client-sdl", client_sdl),
	(prefix + "-" + "tools", tools),
	(prefix + "-" + "devel", devel),
	(prefix + "-" + "devel", devel_r, "-r"),
	("qfcc", qfcc),
	("qfcc", qfcc_r, "-r"),
	("io_mesh_qfmdl", io_mesh_qfmdl),
	("io_mesh_qfmdl", io_mesh_qfmdl_r, "-r"),
]

for p in packages:
	opt = ""
	if len (p) >= 3:
		opt = p[2] + " "
	cmd = "zip -9 " + opt + p[0] + version + ".zip " + " ".join (p[1])
	print cmd
	system (cmd)
