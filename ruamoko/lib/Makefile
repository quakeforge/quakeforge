QFCC=qfcc
QCFLAGS=-qq -g -Werror
QCPPFLAGS=-I../include

libr_source = \
	entity.r	\
	object.r

libr_obj = $(addsuffix .qfo,$(basename $(libr_source)))
libr_dep = $(addsuffix .d,$(basename $(libr_source)))

%.qfo: %.r
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

%.qfo: %.qc
	$(QFCC) $(QCFLAGS) $(QCPPFLAGS) -c -o $@ $<

libr.pak: $(libr_obj)
	pak -cf $@ $^

clean:
	rm -f *.qfo *.pak *.d

-include $(libr_dep)
