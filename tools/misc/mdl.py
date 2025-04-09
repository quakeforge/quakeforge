from struct import *
from pprint import *
import sys

model = open(sys.argv[1],"rb").read()
file_bytes = len(model)
m = unpack ("4s i 3f 3f f 3f i i i i i i i", model[:76])
model = model[76:]
m = m[0:2] + (m[2:5],) + (m[5:8],) + m[8:9] + (m[9:12],) + m[12:20]
if m[1] == 6:
	m = m + unpack ("i f", model[:8])
	model = model[8:]
pprint (m)

print(f"skins @ {file_bytes - len(model)}")
skins = []
s = m[7] * m[8]
for i in range(m[6]):
	t = unpack ("i", model[:4])[0]
	model = model[4:]
	if t == 0:
		skins.append((t,model[:s]))
		model = model[s:]
	else:
		n = unpack ("i", model[:4])[0]
		model = model [4:]
		print (n)
		k = (n, unpack (("%df" % n), model[:n*4]), [])
		model = model [n*4:]
		for j in range (n):
			k[2].append (model[:s])
			model = model[s:]
		skins.append (k)
#pprint (skins)

print(f"stverts @ {file_bytes - len(model)}")
stverts = []
for i in range(m[9]):
	x = unpack ("i i i", model[:12])
	stverts.append ((x[0], x[1:]))
	model = model [12:]
pprint (stverts)

print(f"tris @ {file_bytes - len(model)}")
tris = []
for i in range(m[10]):
	tris.append (unpack ("i i i i", model[:16]))
	tris[-1] = (tris[-1][0], tris[-1][1:])
	model = model [16:]
pprint (tris)

print(f"frames @ {file_bytes - len(model)}")
frames = []
for i in range (m[11]):
	t = unpack ("i", model[:4])[0]
	model = model[4:]
	if t==0:
		if m[1] == 6:
			try:
				x = unpack ("3B B 3B B 16s", model[:24])
			except:
				print(len(model))
				raise
			f = (t, ((x[:3], x[3]), (x[4:7], x[7]), x[8]), [])
			model = model[24:]
		else:
			x = unpack ("3B B 3B B", model[:8])
			f = (t, ((x[:3], x[3]), (x[4:7], x[7])), [])
			model = model[8:]
		for j in range(m[9]):
			x = unpack("3B B", model[:4])
			model = model[4:]
			f[2].append((x[:3], x[3]))
		frames.append(f)
	else:
		g = (t, unpack ("i 3B B 3B B", model[:12]))
		model = model[12:]
		n = g[1][0]
		g = g + (unpack (("%df" % n), model[:n*4]), [])
		model = model[n*4:]
		for k in range (g[1][0]):
			f = (unpack ("3B B 3B B 16s", model[:24]), [])
			model = model[24:]
			for j in range(m[9]):
				x = unpack("3B B", model[:4])
				f[1].append((x[:3], x[3]))
				model = model[4:]
			g[3].append(f)
		frames.append(g)
pprint(frames)
