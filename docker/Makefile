build: Dockerfile Makefile
	docker build -t ubermag/oommf:latest .

push: build
	docker push ubermag/oommf:latest

run:
	docker run -ti -v `pwd`:/io ubermag/oommf:latest bash
