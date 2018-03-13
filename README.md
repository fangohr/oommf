[![Build Status](https://travis-ci.org/fangohr/oommf.svg?branch=master)](https://travis-ci.org/fangohr/oommf)
[![Anaconda-Server Badge](https://anaconda.org/conda-forge/oommf/badges/version.svg)](https://anaconda.org/conda-forge/oommf)

# Object Oriented MicroMagnetic Framework (OOMMF)

This is a repository of the [OOMMF](https://math.nist.gov/oommf/oommf.html) software. It holds the OOMMF source files as distributed by [NIST](https://www.nist.gov/) available [here](https://math.nist.gov/oommf/software-20.html) in the [oommf](oommf) subdirectory of this repository.

The intention is to provide the OOMMF distribution files in a git repository for those who prefer to pull them via git.

## Version

The current version of OOMMF code in this repository is the alpha release of OOMMF 2.0.

## License

The License for the OOMMF code can be found [here](oommf/LICENSE). If you use any of the DMI extensions (that are not part of this repository), please refer to the [3-Clause BSD License](https://opensource.org/licenses/BSD-3-Clause) and licenses in corresponding repositories ([C<sub>nv</sub>](https://github.com/joommf/oommf-extension-dmi-cnv), [T and O](https://github.com/joommf/oommf-extension-dmi-t), or [D<sub>2d</sub>](https://github.com/joommf/oommf-extension-dmi-d2d)).

## Extensions

We provide Dzyaloshinskii-Moriya interaction extensions for three different crystallographic classes:

1. C<sub>nv</sub> ([repository](https://github.com/joommf/oommf-extension-dmi-cnv)),
2. T and O ([repository](https://github.com/joommf/oommf-extension-dmi-t)), and
3. D<sub>2d</sub> ([repository](https://github.com/joommf/oommf-extension-dmi-d2d))

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

...

## How to cite

Information on how to cite the usage of OOMMF can be found [here](https://math.nist.gov/oommf/oommf_cites.html).

If you use any of the DMI extensions we provide in your research, please refer to the "How to cite" section in the corresponding repositories for the particular crystallographic class ([C<sub>nv</sub>](https://github.com/joommf/oommf-extension-dmi-cnv), [T and O](https://github.com/joommf/oommf-extension-dmi-t), or [D<sub>2d</sub>](https://github.com/joommf/oommf-extension-dmi-d2d)).

## Acknowledgements

The repository which holds the OOMMF source code was developed as a part of [OpenDreamKit](http://opendreamkit.org/) – Horizon 2020 European Research Infrastructure project (676541) and the [EPSRC Programme grant on Skyrmionics (EP/N032128/1)](https://www.skyrmions.ac.uk/).

## For developers

To update this repository with a new OOMMF release, please update the variables in `Makefile-update-oommf`, and then run `make -f Makefile-update-oommf all`.

