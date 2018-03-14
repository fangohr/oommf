# Commands prefixed with 'docker-' will execute the relevant task in a
# docker container.
#
# If you have docker, the steps are
#
# 1. docker-build (creates docker image)
# 2. docker-test  (runs a few examples in that docker image container)
# 3. docker-run (open shell, in which oommf commands can be run)
#

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

test:
	@echo "get some other diagnostic data about the environment"
	hostname
	pwd
	echo
	g++ --version
	echo 'puts [info tclversion];exit 0' | tclsh
	echo
	echo 'puts [info patchlevel];exit 0' | tclsh
	echo
	echo "Which OOMMF version?"
	tclsh oommf/oommf.tcl +version
	echo
	echo "We should run some OOMMF examples here"
	echo "TODO"

docker-test:
	@echo "Now we test the OOMMF installation inside Docker:"
	docker run oommfimage cat /etc/issue
	docker run --workdir /usr/local/oommf oommfimage make test


docker-build:
	@echo "Building Docker image with oommf inside"
	docker build -f docker/oommf/Dockerfile -t oommfimage .

docker-run:
	@echo "Fire up container, ready to process 'oommf' command, mount local directory"
	docker run -ti -v `pwd`:/io oommfimage bash
