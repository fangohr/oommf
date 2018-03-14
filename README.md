[![Build Status](https://travis-ci.org/fangohr/oommf.svg?branch=master)](https://travis-ci.org/fangohr/oommf)
[![License Badge](https://img.shields.io/badge/License-OOMMF-blue.svg)](oommf/LICENSE)
[![Anaconda-Server Badge](https://anaconda.org/conda-forge/oommf/badges/version.svg)](https://anaconda.org/conda-forge/oommf)

# Object Oriented MicroMagnetic Framework (OOMMF)

## About

This is a repository of the [OOMMF](https://math.nist.gov/oommf/oommf.html) software. It holds the OOMMF source files as distributed by [NIST](https://www.nist.gov/) available [here](https://math.nist.gov/oommf/software-20.html) in the [oommf](oommf) subdirectory of this repository.

The intention is to provide the OOMMF distribution files in a git repository for those who prefer to pull them via git.

## Version

The current version of OOMMF code in this repository is the alpha release of OOMMF 2.0 (`20a0 20170929 a0`). If you are using `joommf/oommf` Docker image (more details in [Docker](#Docker) section), you can get the currect version of OOMMF by running `oommf-version`. The history of all OOMMF versions hosted in this directory is provided in [versionlog.txt](versionlog.txt).

## Clone and build

If you want to build a clean OOMMF (without externally made extensions), clone the repository:

    git clone https://github.com/fangohr/oommf.git

and build it

    make build


For building OOMMF with different extensions, please refer to the [Extensions](#Extensions) section.

## Extensions

We provide Dzyaloshinskii-Moriya interaction extensions for three different crystallographic classes:

1. C<sub>nv</sub> - interfacial DMI ([repository](https://github.com/joommf/oommf-extension-dmi-cnv)),
2. T(O) - bulk DMI ([repository](https://github.com/joommf/oommf-extension-dmi-t)), and
3. D<sub>2d</sub> - antiskyrmion DMI ([repository](https://github.com/joommf/oommf-extension-dmi-d2d))

After you clone this repository

    git clone https://github.com/fangohr/oommf.git

you can build OOMMF and add a particular extensions by running

    make build-with-dmi-extensions-XXX

where `XXX` can be `cnv`, `t`, or `d2d` for individual extensions. In case you want to build OOMMF with all three DMI extensions, you can run:

    make build-with-dmi-extensions-all

## Docker

In [`docker/`](docker/) directory we provide [Dockerfile](docker/Dockerfile) and [Makefile](docker/Makefile) for building and running Docker images containg OOMMF. In addition, we also provide pre-built image at DockerHub under [`joommf/oommf`](https://hub.docker.com/r/joommf/oommf/) repository. More information about Docker as well as on how to install it on your system can be found [here](https://www.docker.com/).

### Getting the image

If you want to build an image yourself, navigate to the `docker/` directory and run

    make build

This command builds the image under `joommf/oommf:latest` name. Otherwise, you can obtain the most recent image by pulling it from DockerHub [`joommf/oommf`](https://hub.docker.com/r/joommf/oommf/) repository

    docker pull joommf/oommf

### Running the container

To run a container, navigate to `docker/` directory and run

    make run

Once inside the container, the `oommf.tcl` file is in `/usr/local/oommf/oommf/oommf.tcl`. For convenience, we provide a shell script `oommf` in the search path (in `/usr/local/bin`). This can be used, for example:

    root@715477218aac:/io# oommf +version
    <23> oommf.tcl 1.2.0.6  info:
    oommf.tcl 1.2.0.6

In addition, during the build process, we also set an environment variable `OOMMFTCL` to point to the `/usr/local/oommf/oommf/oommf.tcl` file.

### No graphical user interface

Please note that OOMMF's graphical user interface (GUI) in Docker container cannot be used (without further work), but it can be used to execute `.mif` files, through `boxsi`, for example:

    root@715477218aac:/io# oommf boxsi
    
## Conda

We also made recipes for building [Conda](https://www.anaconda.com/) OOMMF package and they are available [here](https://github.com/conda-forge/oommf-feedstock). Information on how to install Conda on your system as well as how to use it can be found [here](https://conda.io/docs/). Installing `oommf` [package](https://anaconda.org/conda-forge/oommf) using conda can be done by running

    conda install oommf

## Support

For support on OOMMF itself, please refer to OOMMF's [web page](https://math.nist.gov/oommf/oommf.html). However, if you have any difficulties or problems in using any of the features we provide in this repository, you are welcome to raise an issue in our [joommf/help](https://github.com/joommf/help) repository.

## License

The License for the OOMMF code can be found [here](oommf/LICENSE). If you use any of the DMI extensions (that are not part of this repository), please refer to the [3-Clause BSD License](https://opensource.org/licenses/BSD-3-Clause) and licenses in corresponding repositories ([C<sub>nv</sub>](https://github.com/joommf/oommf-extension-dmi-cnv), [T(O)](https://github.com/joommf/oommf-extension-dmi-t), or [D<sub>2d</sub>](https://github.com/joommf/oommf-extension-dmi-d2d)).

## How to cite

Information on how to cite the usage of OOMMF can be found [here](https://math.nist.gov/oommf/oommf_cites.html).

If you use any of the DMI extensions we provide in your research, please refer to the "How to cite" section in the corresponding repositories for the particular crystallographic class ([C<sub>nv</sub>](https://github.com/joommf/oommf-extension-dmi-cnv), [T(O)](https://github.com/joommf/oommf-extension-dmi-t), or [D<sub>2d</sub>](https://github.com/joommf/oommf-extension-dmi-d2d)).

## Acknowledgements

The repository which holds the OOMMF source code was developed as a part of [OpenDreamKit](http://opendreamkit.org/) – Horizon 2020 European Research Infrastructure project (676541) and the [EPSRC Programme grant on Skyrmionics (EP/N032128/1)](https://www.skyrmions.ac.uk/).
