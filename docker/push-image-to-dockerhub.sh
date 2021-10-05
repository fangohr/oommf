#!/usr/bin/env bash

# login with docker hub account
docker login

# build image (this will clone github.com/fangohr/oommf master)
# - must make sure that is the right version, or update the
# Dockerfile
docker build --no-cache -t oommf/oommf:20a3 .

# push image to dockerhub
echo "Run 'docker push oommf/oommf:20a3' to push to dockerhub"

# the 'latest' tag is not added by DockerHub as one may guess:
# https://medium.com/@mccode/the-misunderstood-docker-tag-latest-af3babfd6375
# so if we want to allow our users to use 'docker pull oommf//oommf', then we need to define
# the 'latest' tag manually.
#
echo "Also run these commands to set the 'latest' tag to the new image:"
echo "  docker tag oommf/oommf:20a3 oommf/oommf:latest"
echo "  docker push oommf/oommf:latest"
