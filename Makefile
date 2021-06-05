# This Makefile is to build the OOMMF (and extensions)
# that are in this repository / the working directory
#
# To update the OOMMF sources or extensions, please
# use Makefile-update-oommf .
#


OOMMFPREFIX="oommf"

build:
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake distclean
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake upgrade
	cd $(OOMMFPREFIX) && ./oommf.tcl pimake


in-docker: SHELL:=/bin/bash
in-docker:
	docker build --no-cache -t dockertestimage .
	docker run -ti -d --name testcontainer dockertestimage
	docker exec testcontainer make build
	docker exec testcontainer make test-all
	docker stop testcontainer
	docker rm testcontainer

OOMMFTCL=oommf/oommf.tcl

test-all:
	tclsh $(OOMMFTCL) boxsi +fg oommf/app/oxs/examples/stdprob3.mif -exitondone 1
	tclsh $(OOMMFTCL) boxsi +fg oommf/app/oxs/examples/stdprob4.mif -exitondone 1
