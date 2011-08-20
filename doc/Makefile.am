AUTOMAKE_OPTIONS= foreign

SUBDIRS= man

DOX=\
	bind.dox connect.dox cshifts.dox dirconf.dox faq.dox \
	filesystem.dox mapformat.dox property-lists.dox qtv.dox quakeforge.dox \
	qw-cap-spec.dox qw-download-spec.dox surround-sound.dox timestamps.dox

EXTRA_DIST= qf.ico \
	\
	skybox.fig template.c template.h \
	quakeforge.dox.conf.in \
	\
	${DOX} \
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

doc: quakeforge.dox.conf progs/vm-mem.png progs/vm-mem.eps qtv/qwtv.png qtv/qwtv.eps ${DOX}
	doxygen quakeforge.dox.conf
