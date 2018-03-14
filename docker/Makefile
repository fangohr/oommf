build: Dockerfile Makefile
	docker build -t joommf/oommf:latest .

push: build
	docker push joommf/oommf:latest

run:
	docker run -ti -v `pwd`:/io joommf/oommf:latest bash
