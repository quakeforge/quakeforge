import array
import sys

black   = (  0,  0,  0)
blue    = (  0,  0,255)
green   = (  0,255,  0)
cyan    = (  0,255,255)
red     = (255,  0,  0)
magenta = (255,  0,255)
yellow  = (255,255,  0)
white   = (255,255,255)
grey    = (128,128,128)

def div10 (x):
	return x / 5

def plot (x, y, color, blend = 0):
	def lim (x):
		if x > 255:
			x = 255
		return x
	y = height - 1 - y
	p = (y * width + x) * 3
	r, g, b = color
	if blend:
		r += a[p + 0]
		g += a[p + 1]
		b += a[p + 2]
	a[p + 0], a[p + 1], a[p + 2] = lim (r), lim (g), lim (b)

def vline (x, y1, y2, color, blend = 0):
	if y1 == y2:
		plot (x, y1, color, blend)
	elif y1 < y2:
		for y in range (y1, y2 + 1):
			plot (x, y, color, blend)
	else:
		for y in range (y2, y1 + 1):
			plot (x, y, color, blend)

def hline (x1, x2, y, color, blend = 0):
	if x1 == x2:
		plot (x1, y, color, blend)
	elif x1 < x2:
		for x in range (x1, x2 + 1):
			plot (x, y, color, blend)
	else:
		for x in range (x2, x1 + 1):
			plot (x, y, color, blend)

class Frames:
	class iterator:
		def __init__ (self, obj):
			self.obj = obj
			self.cur = 0
			self.max = len (obj)
		def __iter__ (self):
			return self
		def next (self):
			if self.cur == self.max:
				raise StopIteration
			obj = self.obj.times[self.cur]
			self.cur += 1
			return obj
	def __init__ (self, data):
		self.times = map (lambda x: int(x), data)
		min = max = self.times[0]
		sum = 0
		for t in self.times:
			if min > t:
				min = t
			if max < t:
				max = t
			sum = sum + t
		self.min = min
		self.max = max
		self.avg = sum / float (len (self.times))
	def __len__ (self):
		return len (self.times)
	def __getitem__ (self, key):
		if type (key) == type (1):
			return self.times[key]
		else:
			if key.step:
				return self.times[key.start:key.stop:key.step]
			else:
				return self.times[key.start:key.stop]
	def __iter__ (self):
		return Frames.iterator (self)

frames = []
for fname in sys.argv[1:]:
	f = open (fname, "rt")
	lines = f.readlines ()
	f.close ()
	frames.append (Frames (lines))
	print frames[-1].min/1000.0, frames[-1].max/1000.0, frames[-1].avg/1000.0

width = len (frames[0])
max = frames[0].max
for f in frames:
	if width < len (f):
		width = len (f)
	if max < f.max:
		max = f.max

tick_mult = 10
if max < 1000:
	scale = 1.0
	ticks = 10
elif max < 2000:
	scale = 0.5
	ticks = 5
elif max < 4000:
	scale = 0.25
	ticks = 5
	tick_mult = 5
elif max < 5000:
	scale = 0.2
	ticks = 2
elif max < 10000:
	scale = 0.1
	ticks = 10
elif max < 20000:
	scale = 0.05
	ticks = 5
elif max < 40000:
	scale = 0.025
	ticks = 5
	tick_mult = 5
elif max < 50000:
	scale = 0.02
	ticks = 2
elif max < 100000:
	scale = 0.01
	ticks = 10
elif max < 200000:
	scale = 0.005
	ticks = 5
elif max < 400000:
	scale = 0.0025
	ticks = 5
	tick_mult = 5
elif max < 500000:
	scale = 0.002
	ticks = 2
elif max < 1000000:
	scale = 0.001
	ticks = 10
elif max < 2000000:
	scale = 0.0005
	ticks = 5
elif max < 4000000:
	scale = 0.00025
	ticks = 5
	tick_mult = 5
elif max < 5000000:
	scale = 0.0002
	ticks = 2
else:
	scale = 0.0001
	ticks = 10

height = int (max * scale + 0.5) + 1

a = array.array ('B', '\0' * height * width * 3)

hline (0, width - 1, 0, grey)
vline (0, 0, height - 1, grey)
for y in range (0, height - 1, ticks):
	hline (0, 5, y, grey)
for y in range (0, height - 1, ticks * tick_mult):
	hline (0, 10, y, grey)
for x in range (0, width - 1, 100):
	vline (x, 0, 5, grey)

b1 = 0.8
a1 = 1 - b1

for f in frames:
	ravg = 0
	for x in range (len (f)):
		y1 = int (f[x] * scale + 0.5)
		if x < len (f) - 1:
			y2 = int (f[x + 1] * scale + 0.5)
			vline (x, y1, y2, map (div10, yellow), 1)
		plot (x, y1, red)
		ravg = a1 * f[x] + b1 * ravg
		plot (x, int (ravg * scale + 0.5), map (div10, cyan), 1)
	hline (0, len (f) - 1, int (f.avg * scale + 0.5), map (div10, blue), 1)

f = open ("timeframes.ppm", "wb")
f.write ("P6\n")
f.write ("#File written by timeframes.py\n")
f.write (`width` + " " + `height` + "\n")
f.write ("255\n")
f.write (a.tostring ())
f.close ()
