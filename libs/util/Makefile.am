noinst_LIBRARIES = libqfutil.a

libqfutil_a_SOURCES = checksum.c cmd.c crc.c cvar.c hash.c mdfour.c qfplist.c sizebuf.c va.c  zone.c

all-local: ../libqfutil.a

../libqfutil.a: libqfutil.a
	cp $(noinst_LIBRARIES) ..
