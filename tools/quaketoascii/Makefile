CC=gcc
CFLAGS=-I/usr/local/include
LDFLAGS=-L/usr/local/lib -lQFutil

quaketoascii: quaketoascii.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ quaketoascii.c

clean:
	-rm quaketoascii
