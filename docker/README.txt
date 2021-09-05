# Ubermag OOMMF container

The files in this directory create the Docker image containing the OOMMF
installation, which is used by [Ubermag](https://ubermag.github.io) if Ubermag
uses Docker to execute oommf.

This can be ignored by most users.

We have a workflow that creates the container periodically:

[![ubermag-container](https://github.com/fangohr/oommf/actions/workflows/ubermag-container.yml/badge.svg)](https://github.com/fangohr/oommf/actions/workflows/ubermag-container.yml)

Pushing the image to dockerhub (`make push`) is a manual procedure.


