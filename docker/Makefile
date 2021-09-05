build: Dockerfile Makefile
	docker build --no-cache -t ubermag/oommf:latest .

push: build
	docker push ubermag/oommf:latest

run:
	docker run --rm -ti -v `pwd`:/io ubermag/oommf:latest bash

test:
	cp ../oommf/app/oxs/examples/stdprob3.mif .
	docker run --rm -v `pwd`:/io ubermag/oommf:latest oommf boxsi +fg stdprob3.mif -exitondone 1

