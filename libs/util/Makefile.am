noinst_LIBRARIES = libqfutil.a

libqfutil_a_SOURCES = checksum.c cmd.c crc.c cvar.c hash.c info.c link.c math.S mathlib.c mdfour.c msg.c qargs.c qendian.c qfplist.c sizebuf.c va.c  zone.c

all-local: ../libqfutil.a

../libqfutil.a: libqfutil.a
	cp $(noinst_LIBRARIES) ..
