QFCC=qfcc
QCFLAGS=-qq -g -Werror
QCPPFLAGS=-I../include

libr_source = \
	crudefile.r		\
	debug.r			\
	entity.r		\
	infokey.r		\
	math.r			\
	message.r		\
	nq_message.r	\
	object.r		\
	physics.r		\
	qw_message.r	\
	qw_physics.r	\
	qw_sys.r		\
	sound.r			\
	string.r		\
	system.r

libr_obj = $(addsuffix .qfo,$(basename $(libr_source)))
libr_dep = $(addprefix .deps/,$(addsuffix .d,$(basename $(libr_source))))

%.qfo: %.r
	@mkdir -p .deps
	@$(QFCC) -M -q $(QCPPFLAGS) -c $< | sed -e 's/\.r\.o\>/.qfo/' > .deps/`basename $@ .qfo`.d
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

%.qfo: %.qc
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

all: libr.pak

libr.pak: $(libr_obj)
	pak -cf $@ $^

clean:
	rm -f *.qfo *.pak *.d
	rm -rf .deps

-include $(libr_dep)
