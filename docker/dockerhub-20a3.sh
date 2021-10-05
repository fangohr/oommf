#!/usr/bin/env bash

# login with docker hub account
docker login

# build image (this will clone github.com/fangohr/oommf master)
# - must make sure that is the right version, or update the
# Dockerfile
docker build --no-cache -t oommf/oommf:20a3 .

# push image to dockerhub
echo "Run 'docker push oommf/oommf:20a3' to push to dockerhub"
