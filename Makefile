# This Makefile is to build the OOMMF (and extensions)
# that are in this repository / the working directory
#
# To update the OOMMF sources or extensions, please
# use Makefile-update-oommf .
#


OOMMF_ROOT="oommf"
OOMMFTCL=$(OOMMF_ROOT)/oommf.tcl

#
# Compile OOMMF and extensions
#

build:
	cd $(OOMMF_ROOT) && ./oommf.tcl pimake distclean
	cd $(OOMMF_ROOT) && ./oommf.tcl pimake upgrade
	cd $(OOMMF_ROOT) && ./oommf.tcl pimake

# Run some examples as a smoke test

test-all:
	tclsh $(OOMMFTCL) boxsi +fg oommf/app/oxs/examples/stdprob3.mif -exitondone 1
	tclsh $(OOMMFTCL) boxsi +fg oommf/app/oxs/examples/stdprob4.mif -exitondone 1


# Carry out the above steps in a container (for better reproducibility)
in-docker:
	docker build --file Dockerfile --no-cache -t dockertestimage .
	docker run -ti -d --name testcontainer dockertestimage
	docker exec testcontainer make build
	docker exec testcontainer make test-all
	docker stop testcontainer
	docker rm testcontainer

