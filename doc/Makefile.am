AUTOMAKE_OPTIONS= foreign

SUBDIRS= man
EXTRA_DIST= 3dfx.txt CodingStyle glqnotes.txt \
	net_DoS_protection.txt progdefs.q1 progdefs.q2 \
	qe3.ico qf-client-3dfx-cvar.txt qf-client-glx-cvar.txt \
	qf-client-sdl-cvar.txt qf-client-sgl-cvar.txt qf-client-svga-cvar.txt \
	qf-client-x11-cmd.txt qf-client-x11-cvar.txt qf-server-cmd.txt \
	qf-server-cvar.txt quake.gif quake.ico readme.txt \
	skybox.fig template.h timestamps.txt wqreadme.txt \
	quakeforge.dox.in \
	\
	config/glspeed-v1.cfg config/glspeed-v3.cfg config/swspeed.cfg \
	\
	config/gib/adjustvolume.gib config/gib/infobot.gib config/gib/ln.gib \
	config/gib/qfadmin.gib config/gib/sshot.gib config/gib/zoom.gib \
	\
	data/comexp.txt \
	\
	data/help.txt data/licinfo.txt data/manual.txt data/order.txt \
	data/readme.txt data/rlicnse.txt data/slicnse.txt data/techinfo.txt \
	\
	data/docs/install data/docs/install.q2mission \
	data/docs/install.quake data/docs/install.quake2 data/docs/readme \
	data/docs/readme.glquake data/docs/readme.squake data/docs/readme.x11 \
	\
	ideas/quakedownload.txt ideas/rhamph.txt ideas/serverlist.txt \
	\
	progs/vm-mem.fig

%.png: %.fig
	@mkdir -p `dirname $@`
	fig2dev -L png $< $@

%.eps: %.fig
	@mkdir -p `dirname $@`
	fig2dev -L ps $< $@

progs/vm-mem.png: progs/vm-mem.fig
progs/vm-mem.eps: progs/vm-mem.fig

doc: quakeforge.dox progs/vm-mem.png progs/vm-mem.eps
	doxygen quakeforge.dox
