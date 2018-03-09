[![Build Status](https://travis-ci.org/fangohr/oommf.svg?branch=master)](https://travis-ci.org/fangohr/oommf)
[![Anaconda-Server Badge](https://anaconda.org/conda-forge/oommf/badges/version.svg)](https://anaconda.org/conda-forge/oommf)

# oommf

This is a git repository of the Object Oriented MicroMagnetic Framework software 
(available at https://github.com/fangohr/oommf/)

The repository holds the OOMMF source files as distributed from NIST at 
http://math.nist.gov/oommf/software-12.html in the [oommf](https://github.com/fangohr/oommf/tree/master/oommf) subdirectory.

The intention is to provide the OOMMF distribution files in a git repository for those who prefer to pull them via git.

The License for the OOMMF code is available in https://github.com/fangohr/oommf/blob/master/oommf/LICENSE



## For developers

To update this repository with a new OOMMF release, please update the variables in
`Makefile-update-oommf`, and then run `make -f Makefile-update-oommf all`.

