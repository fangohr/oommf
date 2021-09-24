#!/usr/bin/env bash

# login with docker hub account
docker login

# build image
docker build --no-cache -t ubermag/oommf:20a2 .

# push image to dockerhub
docker push ubermag/oommf:20a2
