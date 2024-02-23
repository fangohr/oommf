build: Dockerfile Makefile
	docker build -t oommf59 .
	@# oommf59 is the name of the docker image.

# use this if the container needs to be rebuilt and docker is caching too many steps in the Dockerfile
build-force: Dockerfile Makefile
	docker build --no-cache -t oommf59 .

clonerepo:
	@# This cloned version of the repo will be visible in the container (under /io/repo).
	@# So editing of the files can be done on the host in the ~repo~ directory, and
	@# building can be done inside the container
	git clone https://github.com/fangohr/oommf.git repo
	@# Check out right branch with the modifications (https://github.com/fangohr/oommf/pull/58)
	cd repo && git fetch -a && git checkout add-configuration-for-linux-arm64

run:
	docker run --rm -ti -v `pwd`:/io oommf59 bash
	@# options: --rm : remove docker container when execution stops
              -ti : allow terminal interaction between host and container
              -v : mount the `pwd` Volume on the host to /io in the container



