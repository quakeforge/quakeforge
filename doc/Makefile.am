AUTOMAKE_OPTIONS= foreign

SUBDIRS= man
EXTRA_DIST= qf.ico \
	\
	skybox.fig template.c template.h timestamps.txt \
	quakeforge.dox.in \
	\
	faq.txt quakeforge.txt \
	\
	config/glspeed-v1.cfg config/glspeed-v3.cfg config/swspeed.cfg \
	\
	config/gib/adjustvolume.gib config/gib/infobot.gib config/gib/ln.gib \
	config/gib/qfadmin.gib config/gib/sshot.gib config/gib/zoom.gib \
	\
	progs/vm-exec.c progs/vm-mem.fig

%.png: %.fig
	@mkdir -p `dirname $@`
	fig2dev -L png $< $@

%.eps: %.fig
	@mkdir -p `dirname $@`
	fig2dev -L ps $< $@

clean-local:
	-rm -fr doxygen

progs/vm-mem.png: progs/vm-mem.fig
progs/vm-mem.eps: progs/vm-mem.fig

doc: quakeforge.dox progs/vm-mem.png progs/vm-mem.eps
	doxygen quakeforge.dox
