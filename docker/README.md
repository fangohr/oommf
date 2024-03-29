# Ubermag OOMMF container

The files in this directory create the [Docker image](https://hub.docker.com/r/oommf/oommf) containing the OOMMF
installation, which is used by [Ubermag](https://ubermag.github.io) if Ubermag
uses Docker to execute oommf.

This directory can be ignored by most users, unless: (i) you want to build your own Docker container to provide OOMMF (and you are not happy with the one provided through [oommf/oommf](https://hub.docker.com/u/oommf)), or (ii) you want to update that container with a more recent version of OOMMF or newer/other OOMMF extensions.

We have a workflow that creates the container periodically:

[![ubermag-container](https://github.com/fangohr/oommf/actions/workflows/ubermag-container.yml/badge.svg)](https://github.com/fangohr/oommf/actions/workflows/ubermag-container.yml)

Pushing the image to dockerhub is a manual procedure at the moment (file: [push-image-to-dockerhub.sh](push-image-to-dockerhub.sh)).

See README one level up for more details on how to use the Docker container to run OOMMF.




