noinst_LIBRARIES = libqfutil.a

libqfutil_a_SOURCES = cmd.c cvar.c hash.c mdfour.c sizebuf.c va.c zone.c

all-local: ../libqfutil.a

../libqfutil.a: libqfutil.a
	cp $(noinst_LIBRARIES) ..
