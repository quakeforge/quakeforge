EXE=qwaq

MAKEDEPS=$(CC) -MM $(CPPFLAGS) $< | sed -e 's/$*\.o:*/$*\.o $@:/g' > $@

CFLAGS+=-Wall -Werror -g -O2
CPPFLAGS+=
#LDFLAGS=-static
LIBS=
vpath %.a

SRC=main.c builtins.c

%.d: %.c
	$(MAKEDEPS)

OBJ=$(patsubst %,%.o,$(basename $(SRC)))
DEP=$(patsubst %.o,%.d,$(OBJ))

all: $(EXE) $(EXE).dat

$(EXE): $(OBJ) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $^ ../../d.o/libs/gamecode/engine/.libs/libQFgamecode.a -lQFutil -lz -lm

$(EXE).dat: progs.src *.qc ../../cs-code/object.r *.r
	qfcc --warn=error --code=debug

clean:
	rm -f $(EXE) *.o *.d core *.dat *.sym progdefs.h

-include $(DEP)
