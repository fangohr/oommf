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
	g++ --version
	echo 'puts $tcl_version;exit 0' | tclsh
	echo 'puts [info patchlevel];exit 0' | tclsh
	cat /etc/issue

	echo "Which OOMMF version?"
	tclsh /usr/local/oommf/oommf/oommf.tcl +version
	echo "short command available?"
	oommf +version

	echo "We should run some OOMMF examples here"
	echo "TODO"


docker-test:
	@echo "Now we test the OOMMF installation inside Docker:"
	docker run oommfimage make -f /usr/local/oommf/Makefile test


docker-build:
	@echo "Building Docker image with oommf inside"
	docker build -f docker/oommf/Dockerfile -t oommfimage .


run: docker-run

docker-run:
	@echo "Fire up container, ready to process 'oommf' command, mount local directory"
	docker run -ti -v `pwd`:/io oommfimage bash
