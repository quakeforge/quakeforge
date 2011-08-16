AUTOMAKE_OPTIONS= foreign

SUBDIRS= man

TXT_DOCS=\
	bind.txt connect.txt cshifts.txt dirconf.txt faq.txt \
	filesystem.txt mapformat.txt qtv.txt quakeforge.txt qw-cap-spec.txt \
	qw-download-spec.txt surround-sound.txt timestamps.txt

EXTRA_DIST= qf.ico \
	\
	skybox.fig template.c template.h \
	quakeforge.dox.in \
	\
	${TXT_DOCS} \
	\
	config/glspeed-v1.cfg config/glspeed-v3.cfg config/swspeed.cfg \
	\
	config/gib/adjustvolume.gib config/gib/infobot.gib config/gib/ln.gib \
	config/gib/qfadmin.gib config/gib/sshot.gib config/gib/zoom.gib \
	\
	progs/vm-exec.c progs/vm-mem.fig \
	qtv/qwtv.fig

SUFFIXES=.eps .fig .png
.fig.png:
	@mkdir -p `dirname $@`
	fig2dev -L png $< $@

.fig.eps:
	@mkdir -p `dirname $@`
	fig2dev -L ps $< $@

clean-local:
	-rm -fr doxygen

progs/vm-mem.png: progs/vm-mem.fig
progs/vm-mem.eps: progs/vm-mem.fig
qtv/qwtv.png: qtv/qwtv.fig
qtv/qwtv.eps: qtv/qwtv.fig

doc: quakeforge.dox progs/vm-mem.png progs/vm-mem.eps qtv/qwtv.png qtv/qwtv.eps ${TXT_DOCS}
	doxygen quakeforge.dox
