all: menu.dat

menu.dat: menu.src menu.qc
	qfcc -gP menu.src

clean:
	rm -f *.dat *.h *.sym
