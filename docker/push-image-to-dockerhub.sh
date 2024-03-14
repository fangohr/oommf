#!/usr/bin/env bash

set -ex

# login with docker hub account
echo "Run 'docker login'"
docker login

echo "Build-multi-architecture container:"

# if no builder exists yet, then create one. We use the exit code of `docker buildx inspect` to decide
# (source: https://github.com/docker/buildx/discussions/1319)
docker buildx inspect container || echo docker buildx create --name container --driver=docker-container

# multi-platform build and push to Dockerhub
time docker buildx build --tag oommf/oommf:latest tag --tag oommf/oommf:21a0 --platform linux/arm64,linux/amd64 --builder container --push .

