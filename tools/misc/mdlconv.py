from struct import *
from pprint import *
import sys
import os

def convert (fname):
	f = open (fname, "rb")
	model = f.read ()
	f.close ()
	if len (model) < 8 or model[:4] != "IDPO" or unpack ("<I", model[4:8])[0] != 3:
		print fname, "is not a v3 mdl file";
		return
	os.rename (fname, fname + ".bak")
	f = open (fname, "wb")
	f.write (model[:4] + pack ("<I", 6) + model[8:76])
	f.write ('\0' * 8)
	numskins, skinwidth, skinheight, numverts, numtris, numframes = unpack("<IIIIII", model[48:72])
	model = model[76:]

	size = skinwidth * skinheight
	for i in range (numskins):
		type = unpack ("<I", model[:4])[0]
		f.write (model[:4])
		model = model[4:]
		if not type:
			f.write (model[:size])
			model = model [size:]
		else:
			num = unpack ("<I", model[:4])[0]
			f.write (model[:4])
			model = model[4:]
			f.write (model[:num * 4])
			model = model[num * 4:]
			f.write (model[:size * num])
			model = model[size * num:]

	for i in range (numverts):
		s = model[:12]
		model = model[12:]
		fl = unpack("<I", s[:4])[0] << 5
		s = pack("<I", fl) + s[4:]
		f.write (s)


	f.write (model[:numtris * 16])
	model = model[numtris * 16:]

	for i in range (numframes):
		type = unpack ("<I", model[:4])[0]
		f.write (model[:4])
		model = model[4:]
		if not type:
			f.write (model[:8])
			model = model [8:]
			f.write ('\0' * 16)
			f.write (model[:numverts * 4])
			model = model [numverts * 4:]
		else:
			print "don't know frame groups yet"
			sys.exit (1)

	f.close ()

for f in sys.argv[1:]:
	print f
	convert (f)
