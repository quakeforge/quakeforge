QFCC=qfcc
QCFLAGS=-qq -g -Werror
QCPPFLAGS=-I../include

game_source = \
	axe.r		\
	gameent.r	\
	message.r	\
	sound.r		\
	tempent.r	\
	trace.r		\
	vector.r	\
	world.r

game_obj = $(addsuffix .qfo,$(basename $(game_source)))
game_dep = $(addsuffix .d,$(basename $(game_source)))

%.qfo: %.r
	@$(QFCC) -M $(QCPPFLAGS) -c $< | sed -e 's/\.r\.o\>/.qfo/' > `basename $@ .qfo`.d
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

%.qfo: %.qc
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

game.dat: $(game_obj) ../lib/libr.pak
	$(QFCC) -qq -g -o $@ $(game_obj) -l../lib/libr.pak

clean:
	rm -f *.qfo *.dat *.sym *.d

-include $(game_dep)
