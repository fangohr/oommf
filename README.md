[![Build Status](https://travis-ci.org/fangohr/oommf.svg?branch=master)](https://travis-ci.org/fangohr/oommf)
[![License Badge](https://img.shields.io/badge/License-OOMMF-blue.svg)](oommf/LICENSE)
[![Anaconda-Server Badge](https://anaconda.org/conda-forge/oommf/badges/version.svg)](https://anaconda.org/conda-forge/oommf)

# Object Oriented MicroMagnetic Framework (OOMMF)

## About

This is a repository of the [OOMMF](https://math.nist.gov/oommf/oommf.html) software. It holds the OOMMF source files as distributed by [NIST](https://www.nist.gov/) available [here](https://math.nist.gov/oommf/software-20.html) in the [oommf](oommf) subdirectory of this repository.

The intention is to provide the OOMMF distribution files in a git repository for those who prefer to pull them via git.

## Version

The current version of OOMMF code in this repository is the alpha release of OOMMF 2.0. For further information, please refer to [versionlog.txt](versionlog.txt).

## License

The License for the OOMMF code can be found [here](oommf/LICENSE). If you use any of the DMI extensions (that are not part of this repository), please refer to the [3-Clause BSD License](https://opensource.org/licenses/BSD-3-Clause) and licenses in corresponding repositories ([C<sub>nv</sub>](https://github.com/joommf/oommf-extension-dmi-cnv), [T(O)](https://github.com/joommf/oommf-extension-dmi-t), or [D<sub>2d</sub>](https://github.com/joommf/oommf-extension-dmi-d2d)).

## Clone and build

If you want to build a clean OOMMF (without externally made extensions), clone the repository:
```
git clone https://github.com/fangohr/oommf.git
```
and build it
```
make build
```

For building OOMMF with different extensions, please refer to the [Extensions](#Extensions) section.

## Extensions

We provide Dzyaloshinskii-Moriya interaction extensions for three different crystallographic classes:

1. C<sub>nv</sub> - interfacial DMI ([repository](https://github.com/joommf/oommf-extension-dmi-cnv)),
2. T(O) - bulk DMI ([repository](https://github.com/joommf/oommf-extension-dmi-t)), and
3. D<sub>2d</sub> - antiskyrmion DMI ([repository](https://github.com/joommf/oommf-extension-dmi-d2d))

After you clone this repository
```
git clone https://github.com/fangohr/oommf.git
```
you can build OOMMF and add a particular extensions by running
```
make build-with-dmi-extensions-XXX
```
where `XXX` can be `cnv`, `t`, or `d2d` for individual extensions. In case you want to build OOMMF with all three DMI extensions, you can run:
```
make build-with-dmi-extensions-all
```

## Conda

...

## Docker

### Creating the image

The makefile can be used to build a new image, and push it to hub docker.

### Using the image

Users should be able to install docker on their computer, and then run

    docker pull joommf/oommf

to download the image to their computer. To get access to OOMMF, they
should run:

    docker run -ti -v `pwd`:/io joommf/oommf bash

Once inside the container, the `oommf.tcl` file is in
`/usr/local/oommf/oommf/oommf.tcl`. For convenience, we provide a
shell script `oommf` in the search path (in `/usr/local/bin`). This can be used, for example:

    root@715477218aac:/io# oommf +version
    <23> oommf.tcl 1.2.0.6  info:
    oommf.tcl 1.2.0.6
    root@715477218aac:/io# 


### No graphical user interface

Note that OOMMF's graphical user interface cannot be used (without further work). It can be used to execute mif files, through boxsi, for example:

    root@715477218aac:/io# oommf boxsi 

## How to cite

Information on how to cite the usage of OOMMF can be found [here](https://math.nist.gov/oommf/oommf_cites.html).

If you use any of the DMI extensions we provide in your research, please refer to the "How to cite" section in the corresponding repositories for the particular crystallographic class ([C<sub>nv</sub>](https://github.com/joommf/oommf-extension-dmi-cnv), [T(O)](https://github.com/joommf/oommf-extension-dmi-t), or [D<sub>2d</sub>](https://github.com/joommf/oommf-extension-dmi-d2d)).

## Acknowledgements

The repository which holds the OOMMF source code was developed as a part of [OpenDreamKit](http://opendreamkit.org/) – Horizon 2020 European Research Infrastructure project (676541) and the [EPSRC Programme grant on Skyrmionics (EP/N032128/1)](https://www.skyrmions.ac.uk/).

## For developers

To update this repository with a new OOMMF release, please update the variables in `Makefile-update-oommf`, and then run `make -f Makefile-update-oommf all`.

