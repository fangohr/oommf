
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
