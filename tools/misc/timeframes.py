import array

black   = (  0,  0,  0)
blue    = (  0,  0,255)
green   = (  0,255,  0)
cyan    = (  0,255,255)
red     = (255,  0,  0)
magenta = (255,  0,255)
yellow  = (255,255,  0)
white   = (255,255,255)

def plot (x, y, color):
	y = height - 1 - y
	p = (y * width + x) * 3
	a[p + 0], a[p + 1], a[p + 2] = color

def vline (x, y1, y2, color):
	if y1 == y2:
		plot (x, y1, color)
	elif y1 < y2:
		for y in range (y1, y2 + 1):
			plot (x, y, color)
	else:
		for y in range (y2, y1 + 1):
			plot (x, y, color)

def hline (x1, x2, y, color):
	if x1 == x2:
		plot (x1, y, color)
	elif x1 < x2:
		for x in range (x1, x2 + 1):
			plot (x, y, color)
	else:
		for x in range (x2, x1 + 1):
			plot (x, y, color)

f = open ("timeframes.txt", "rt")
lines = f.readlines ()
f.close

times = map (lambda x: (int(x) + 5)/100, lines)

sum = 0
min=max=times[0]
for t in times:
	if min > t:
		min = t
	if max < t:
		max = t
	sum = sum + t

print min/10.0, max/10.0, sum/10.0 / len(times)
average = sum / len(times)

group = 1
width = (len (times) + group - 1) / group
height = max + 1
a = array.array ('B', '\0' * height * width * 3)

hline (0, width - 1, 0, white)
vline (0, 0, height - 1, white)
for y in range (0, height - 1, 10):
	hline (0, 3, y, white)
for y in range (0, height - 1, 100):
	hline (0, 6, y, white)
for x in range (0, width - 1, 100):
	vline (x, 0, 3, white)

ravg = 0.0
ravg_span = 20
for x in range (width):
	if group > 1:
		tset = times[x * group: x * group + group]
		sum = 0;
		min = max = tset[0]
		for t in tset:
			if min > t:
				min = t
			if max < t:
				max = t
			sum = sum + t
		if x:
			oldavg = avg
		avg = sum / len (tset)
		vline (x, min, max, green)
		if x:
			vline (x, oldavg, avg, yellow)
		else:
			plot (x, avg, yellow)
	else:
		if x < width - 1:
			vline (x, times[x], times[x + 1], yellow)
		plot (x, times[x], red)
		plot (x, int (ravg + 0.5), cyan)
		if (x >= ravg_span):
			ravg -= times[x - ravg_span]/float(ravg_span)
		ravg += times[x]/float(ravg_span)
hline (0, width - 1, average, blue)

f = open ("timeframes.ppm", "wb")
f.write ("P6\n")
f.write ("#File written by timeframes.py\n")
f.write (`width` + " " + `height` + "\n")
f.write ("255\n")
f.write (a.tostring ())
f.close ()
