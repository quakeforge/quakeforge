SUBDIRS=lib game
all clean:
	@for subdir in $(SUBDIRS); do ( \
	  cd $$subdir && $(MAKE) $@ \
	) done
