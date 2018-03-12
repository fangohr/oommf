OOMMFPREFIX="oommf"
DMICNVREPO="oommf-extension-dmi-cnv"
DMITREPO="oommf-extension-dmi-t"
DMID2DREPO="oommf-extension-dmi-d2d"

build:
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake distclean
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake upgrade
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake

build-with-dmi-extension-cnv: get-extension-dmi-cnv build

build-with-dmi-extension-t: get-extension-dmi-t build

build-with-dmi-extension-d2d: get-extension-dmi-d2d build

build-with-dmi-extension-all: get-extension-dmi-cnv get-extension-dmi-d2d get-extension-dmi-t build

get-extension-dmi-cnv:
	git clone https://github.com/joommf/$(DMICNVREPO).git
	cp $(DMICNVREPO)/src/* oommf/app/oxs/local/
	rm -rf $(DMICNVREPO)

get-extension-dmi-t:
	git clone https://github.com/joommf/$(DMITREPO).git
	cp $(DMITREPO)/src/* oommf/app/oxs/local/
	rm -rf $(DMITREPO)

get-extension-dmi-d2d:
	git clone https://github.com/joommf/$(DMID2DREPO).git
	cp $(DMID2DREPO)/src/* oommf/app/oxs/local/
	rm -rf $(DMID2DREPO)
