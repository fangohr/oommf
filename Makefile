OOMMFPREFIX="oommf"
DMICNVREPO="oommf-extension-dmi-cnv"
DMITREPO="oommf-extension-dmi-t"
DMID2DREPO="oommf-extension-dmi-d2d"
MELREPO="oommf-mel"

build:
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake distclean
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake upgrade
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake

build-with-cnv: get-cnv build

build-with-t: get-t build

build-with-d2d: get-d2d build

build-with-mel: get-mel build

build-with-all: get-cnv get-d2d get-t get-mel build

get-cnv:
	git clone https://github.com/joommf/$(DMICNVREPO).git
	cp $(DMICNVREPO)/src/* oommf/app/oxs/local/
	rm -rf $(DMICNVREPO)

get-t:
	git clone https://github.com/joommf/$(DMITREPO).git
	cp $(DMITREPO)/src/* oommf/app/oxs/local/
	rm -rf $(DMITREPO)

get-d2d:
	git clone https://github.com/joommf/$(DMID2DREPO).git
	cp $(DMID2DREPO)/src/* oommf/app/oxs/local/
	rm -rf $(DMID2DREPO)

get-mel:
	git clone https://github.com/yuyahagi/$(MELREPO).git
	cp $(MELREPO)/*.cc oommf/app/oxs/local/
	cp $(MELREPO)/*.h oommf/app/oxs/local/
	rm -rf $(MELREPO)

travis-build: SHELL:=/bin/bash
travis-build:
	docker build --no-cache -t dockertestimage .
	docker run -ti -d --name testcontainer dockertestimage
	docker exec testcontainer make test-all
	docker stop testcontainer
	docker rm testcontainer

test-all:
	tclsh $(OOMMFTCL) boxsi +fg oommf/app/oxs/examples/stdprob3.mif -exitondone 1
	tclsh $(OOMMFTCL) boxsi +fg oommf/app/oxs/examples/stdprob4.mif -exitondone 1

test-all-actions:
	tclsh oommf/oommf.tcl boxsi +fg oommf/app/oxs/examples/stdprob3.mif -exitondone 1
	tclsh oommf/oommf.tcl boxsi +fg oommf/app/oxs/examples/stdprob4.mif -exitondone 1
