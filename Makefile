OOMMFPREFIX="oommf"

build-unix-clean:
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake distclean
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake upgrade
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake



