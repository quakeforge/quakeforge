QFCC=qfcc
QCFLAGS=-qq -g -Werror
QCPPFLAGS=-I../include

game_source = \
	axe.r		\
	gameent.r	\
	tempent.r	\
	world.r

game_obj = $(addsuffix .qfo,$(basename $(game_source)))
game_dep = $(addprefix .deps/,$(addsuffix .d,$(basename $(game_source))))

%.qfo: %.r
	@mkdir -p .deps
	@$(QFCC) -q -M $(QCPPFLAGS) -c $< | sed -e 's/\(\.r\)\?\.o\>/.qfo/' > .deps/`basename $@ .qfo`.d
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

%.qfo: %.qc
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

all: game.dat

game.dat: $(game_obj) ../lib/libr.pak
	$(QFCC) -qq -g -o $@ $(game_obj) -l../lib/libr.pak

clean:
	rm -f *.qfo *.dat *.sym *.d
	rm -rf .deps

-include $(game_dep)
