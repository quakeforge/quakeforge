all: menu.dat

menu.dat: menu.src menu.qc
	qfcc -g -I../include -P menu.src

clean:
	rm -f core *.dat *.h *.sym
