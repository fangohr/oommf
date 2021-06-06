build: Dockerfile Makefile
	docker build --no-cache -t ubermag/oommf:latest .

push: build
	docker push ubermag/oommf:latest

run:
	docker run --rm -ti -v `pwd`:/io ubermag/oommf:latest bash
