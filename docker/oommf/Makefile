image:
	docker build -t joommf/oommf:latest .

push: image
	docker push joommf/oommf

run: image
	docker run -ti -v `pwd`:/io joommf/oommf bash




