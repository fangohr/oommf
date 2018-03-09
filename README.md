[![Build Status](https://travis-ci.org/fangohr/oommf.svg?branch=master)](https://travis-ci.org/fangohr/oommf)
[![Anaconda-Server Badge](https://anaconda.org/conda-forge/oommf/badges/version.svg)](https://anaconda.org/conda-forge/oommf)

# Object Oriented MicroMagnetic Framework (OOMMF)

This is a repository of the [OOMMF](https://math.nist.gov/oommf/oommf.html) software. It holds the OOMMF source files as distributed by [NIST](https://www.nist.gov/) available [here](https://math.nist.gov/oommf/software-20.html) in the [oommf](oommf) subdirectory of this repository.

The intention is to provide the OOMMF distribution files in a git repository for those who prefer to pull them via git.

## License

The License for the OOMMF code can be found [here](oommf/LICENSE).

## For developers

To update this repository with a new OOMMF release, please update the variables in
`Makefile-update-oommf`, and then run `make -f Makefile-update-oommf all`.

