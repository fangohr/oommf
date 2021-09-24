#!/usr/bin/env bash

# login with docker hub account
docker login

# build image
docker build --no-cache -t oommf/oommf:20a2 .

# push image to dockerhub
docker push oommf/oommf:20a2
