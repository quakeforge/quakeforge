noinst_LIBRARIES = libutil.a

libutil_a_SOURCES = cmd.c cvar.c hash.c mdfour.c sizebuf.c va.c zone.c

all-local: ../libutil.a

../libutil.a: libutil.a
	cp $(noinst_LIBRARIES) ..
