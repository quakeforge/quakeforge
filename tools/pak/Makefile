EXE=pak

MAKEDEPS=$(CC) -MM $(CPPFLAGS) $< | sed -e 's/$*\.o:*/$*\.o $@:/g' > $@

CFLAGS+=-Wall -Werror -g -O2
CPPFLAGS+=
#LDFLAGS=-static
LIBS=
vpath %.a

SRC=pak.c pakfile.c

%.d: %.c
	$(MAKEDEPS)

OBJ=$(patsubst %,%.o,$(basename $(SRC)))
DEP=$(patsubst %.o,%.d,$(OBJ))

all: $(EXE)

$(EXE): $(OBJ) $(LIBS)
	$(CC) $(LDFLAGS) -o $@ $^ -lQFutil -lz -lm

clean:
	rm -f $(EXE) *.o *.d core

-include $(DEP)
