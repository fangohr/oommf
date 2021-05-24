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

build-with-all: get-all build

get-cnv:
	python3 clone-log-and-extract-src.py https://github.com/joommf/$(DMICNVREPO).git src

get-t:
	python3 clone-log-and-extract-src.py https://github.com/joommf/$(DMITREPO).git src

get-d2d:
	python3 clone-log-and-extract-src.py https://github.com/joommf/$(DMID2DREPO).git src

get-mel:
	python3 clone-log-and-extract-src.py https://github.com/yuyahagi/$(MELREPO).git .

get-all: get-cnv get-d2d get-t get-mel 


in-docker: SHELL:=/bin/bash
in-docker:
	docker build --no-cache -t dockertestimage .
	docker run -ti -d --name testcontainer dockertestimage
	docker exec testcontainer make test-all
	docker stop testcontainer
	docker rm testcontainer

test-all:
	tclsh $(OOMMFTCL) boxsi +fg oommf/app/oxs/examples/stdprob3.mif -exitondone 1
	tclsh $(OOMMFTCL) boxsi +fg oommf/app/oxs/examples/stdprob4.mif -exitondone 1
