EXE=qwaq

MAKEDEPS=$(CC) -MM $(CPPFLAGS) $< | sed -e 's/$*\.o:*/$*\.o $@:/g' > $@

CFLAGS+=-Wall -Werror -g -O2
CPPFLAGS+=
LIBS=
vpath %.a

SRC=main.c builtins.c

%.d: %.c
	$(MAKEDEPS)

OBJ=$(patsubst %,%.o,$(basename $(SRC)))
DEP=$(patsubst %.o,%.d,$(OBJ))

all: $(EXE) $(EXE).dat

$(EXE): $(OBJ) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $^ -lQFgamecode -lQFutil

$(EXE).dat: progs.src *.qc
	qfcc

clean:
	rm -f $(EXE) *.o *.d core *.dat progdefs.h

-include $(DEP)
