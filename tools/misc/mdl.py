from struct import *
from pprint import *

model = open("flame.mdl","rb").read()
m = unpack ("4s l 3f 3f f 3f i i i i i i i i f", model[:84])
m = m[0:2] + (m[2:5],) + (m[5:8],) + m[8:9] + (m[9:12],) + m[12:21]
model = model[84:]
pprint (m)

skins = []
s = m[7] * m[8]
for i in range(m[6]):
	t=unpack ("l", model[:4])[0]
	model = model[4:]
	if t==0:
		skins.append((t,model[:s]))
		model = model[s:]
	else:
		n = unpack ("l", model[:4])[0]
		model = model [4:]
		k = (n, unpack (`n`+"f", model[:n*4]), [])
		model = model [n*4:]
		for j in range (n):
			k[2].append (model[:s])
			model = model[s:]
		skins.append (k)
#pprint (skins)

stverts = []
for i in range(m[9]):
	stverts.append (unpack ("l l l", model[:12]))
	model = model [12:]
#pprint (stverts)

tris = []
for i in range(m[10]):
	tris.append (unpack ("l l l l", model[:16]))
	tris[-1] = (tris[-1][0], tris[-1][1:])
	model = model [16:]
#pprint (tris)

frames = []
for i in range (m[11]):
	t = unpack ("l", model[:4])[0]
	model = model[4:]
	if t==0:
		f = (t, unpack ("3B B 3B B 16s", model[:24]), [])
		model = model[24:]
		for j in range(m[9]):
			f[2].append(unpack("3B B", model[:4]))
			model = model[4:]
		frames.append(f)
	else:
		g = (t, unpack ("l 3B B 3B B", model[:12]))
		model = model[12:]
		n = g[1][0]
		g = g + (unpack (`n`+"f", model[:n*4]), [])
		model = model[n*4:]
		for k in range (g[1][0]):
			f = (unpack ("3B B 3B B 16s", model[:24]), [])
			model = model[24:]
			for j in range(m[9]):
				f[1].append(unpack("3B B", model[:4]))
				model = model[4:]
			g[3].append(f)
		frames.append(g)
pprint(frames)
