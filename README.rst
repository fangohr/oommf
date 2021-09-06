===============================================
Object Oriented MicroMagnetic Framework (OOMMF)
===============================================


+-----------------------------------+-----------------------------------+
| Description                       | Badge                             |
+===================================+===================================+
| Build (this repo) on latest Ubuntu| |on-ubuntu-latest|                |
+-----------------------------------+-----------------------------------+
| Build (this repo) on OSX          | |badge-osx|                       |
+-----------------------------------+-----------------------------------+
| Release on conda-forge            | |Anaconda-Server Badge|           |
+-----------------------------------+-----------------------------------+
| License                           | |License Badge|                   |
+-----------------------------------+-----------------------------------+

.. sectnum::

.. contents:: 

OOMMF Git repository
====================


About
-----

This is a repository of the
`OOMMF <https://math.nist.gov/oommf/oommf.html>`__ software. It holds
the OOMMF source files as distributed by
`NIST <https://www.nist.gov/>`__ available
`here <https://math.nist.gov/oommf/software.html>`__ in the
`oommf <oommf>`__ subdirectory of this repository.

The intention is to

1. provide the OOMMF distribution files in a git repository for those who prefer to pull them via git

2. include additional OOMMF extensions, which have not made it into the official distribution from NIST into this git repository (see below).

In addition to the OOMMF source code that is offered from NIST (and the
OOMMF extension coming from NIST), this repository contains some
additional user-contributed extensions. Those can be seen from the
`Makefile-update-oommf <Makefile-update-oommf>`__. See also
`Extensions <#Extensions>`__ below.

Compilation of OOMMF (for example by running ``make build``) is expected
to work on Linux and OS X.

Version
-------

The current version of OOMMF code in this repository is the alpha
release of OOMMF 2.0 (``20a2 20200608 a2``).

You can check which version you have, by inspecting the content of the
`oommf-version <oommf-version>`__ file.

The history of all OOMMF versions hosted in this directory is provided
in `versionlog.txt <versionlog.txt>`__.

Clone and build OOMMF from source
---------------------------------

If you want to build OOMMF from source, clone the repository:

::

   git clone https://github.com/fangohr/oommf.git

and build it

::

   make build

Smoke test
----------

As a quick test of the compilation, you can compute standard problems 3
and 4 as a test of your build by running:

::

   make test-all

Extensions
----------

A number of OOMMF extensions are included:

We provide Dzyaloshinskii-Moriya interaction extensions for three
different crystallographic classes:

1. Cnv - interfacial DMI
   (`repository <https://github.com/joommf/oommf-extension-dmi-cnv>`__),
2. T(O) - bulk DMI
   (`repository <https://github.com/joommf/oommf-extension-dmi-t>`__),
   and
3. D2d - antiskyrmion DMI
   (`repository <https://github.com/joommf/oommf-extension-dmi-d2d>`__)

And

-  the OOMMF Magnetoelastic Extension Module
   (`repository <https://github.com/yuyahagi/oommf-mel>`__) from Yu Yahagi.

Docker
------

In the `docker <docker/>`__ directory we provide
`Dockerfile <docker/Dockerfile>`__ and `Makefile <docker/Makefile>`__
for building and running Docker images containg OOMMF. In addition, we
also provide pre-built image at DockerHub under
`ubermag/oommf <https://hub.docker.com/r/ubermag/oommf/>`__
repository. More information about Docker as well as on how to install
it on your system can be found `here <https://www.docker.com/>`__. This
Docker image is also used by
`Ubermag <https://ubermag.github.io/installation.html#how-does-ubermag-find-oommf>`__.

Getting the image
~~~~~~~~~~~~~~~~~

If you want to build an image yourself, navigate to the `docker/ <docker>`__
directory and run

::

   make build

This command builds the image under the ``ubermag/oommf:latest`` name.
Otherwise, you can obtain the most recent image by pulling it from
DockerHub
`ubermag/oommf <https://hub.docker.com/r/ubermag/oommf/>`__
repository

::

   docker pull ubermag/oommf

Running the container
~~~~~~~~~~~~~~~~~~~~~

To run a container, navigate to `docker <docker>`__ directory and run

::

   make run

Or you can type the full command yourself:

::

   docker run --rm -ti -v `pwd`:/io ubermag/oommf:latest 

Once inside the container, the ``oommf.tcl`` file is in
``/usr/local/oommf/oommf/oommf.tcl``. For convenience, we provide a
shell script ``oommf`` in the search path (in ``/usr/local/bin``). This
can be used, for example:

::

   oommfuser@715477218aac:/io# oommf +version
   <7> oommf.tcl 2.0a1  info:
   oommf.tcl 2.0a1

You can also add oommf commands to the command line from the host, for
example:

::

   docker run --rm -ti -v `pwd`:/io ubermag/oommf:latest oommf +version
   <7> oommf.tcl 2.0a1  info:
   oommf.tcl 2.0a1

The current working directory in the host is shared with the ``/io``
directory in the container: this can be used to exchange ``mif`` files
and data files with the container environment: create ``mif`` file in
host, run docker to tell oommf to process the file and create data
files, then analyse data files on the host.

During the build process of the container, we also set an environment
variable ``OOMMFTCL`` to point to the
``/usr/local/oommf/oommf/oommf.tcl`` file. This is used by Ubermag, for
example, to find the ``oommf.tcl`` file.

There is also the ``OOMMF_ROOT`` variable which points to the base
directory of the OOMMF sources (that’s currently
``/usr/local/oommf/oommf``). It can be used, for example, to execute an
OOMMF example:

::

   oommf boxsi +fg $OOMMF_ROOT/app/oxs/examples/stdprob3.mif -exitondone 1

No graphical user interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Please note that OOMMF’s graphical user interface (GUI) in Docker
container cannot be used (without further work), but it can be used to
execute ``.mif`` files, through ``boxsi``, for example:

::

   root@715477218aac:/io# oommf boxsi

Conda
-----

We also made recipes for building `Conda <https://www.anaconda.com/>`__
OOMMF package and they are available
`here <https://github.com/conda-forge/oommf-feedstock>`__. Information
on how to install Conda on your system as well as how to use it can be
found `here <https://conda.io/docs/>`__. Installing ``oommf``
`package <https://anaconda.org/conda-forge/oommf>`__ using conda can be
done by running

::

   conda install -c conda-forge oommf

Support
-------

For support on OOMMF itself, please refer to OOMMF’s `web
page <https://math.nist.gov/oommf/oommf.html>`__. However, if you have
any difficulties or problems in using any of the features we provide in
this repository, you are welcome to raise an issue in our
`ubermag/help <https://github.com/ubermag/help>`__ repository.

License
-------

The License for the OOMMF code can be found `here <oommf/LICENSE>`__. If
you use any of the DMI extensions (that are not part of this
repository), please refer to the `3-Clause BSD
License <https://opensource.org/licenses/BSD-3-Clause>`__ and licenses
in corresponding repositories
(`Cnv <https://github.com/joommf/oommf-extension-dmi-cnv>`__,
`T(O) <https://github.com/joommf/oommf-extension-dmi-t>`__,
`D2d <https://github.com/joommf/oommf-extension-dmi-d2d>`__, and
`magnetoelastic <https://github.com/yuyahagi/oommf-mel>`__).

How to cite
-----------

Information on how to cite the usage of OOMMF can be found
`here <https://math.nist.gov/oommf/oommf_cites.html>`__.

If you use any of the DMI extensions we provide in your research, please
refer to the “How to cite” section in the corresponding repositories for
the particular crystallographic class
(`Cnv <https://github.com/joommf/oommf-extension-dmi-cnv>`__,
`T(O) <https://github.com/joommf/oommf-extension-dmi-t>`__, or
`D2d <https://github.com/joommf/oommf-extension-dmi-d2d>`__).

Acknowledgements
----------------

The repository which holds the OOMMF source code was developed as a part
of `OpenDreamKit <http://opendreamkit.org/>`__ – Horizon 2020 European
Research Infrastructure project (676541) and the `EPSRC Programme grant
on Skyrmionics (EP/N032128/1) <https://www.skyrmions.ac.uk/>`__. Further
thanks go to the University of Southampton and the Max Planck Institute
for the Structure and Dynamics of Matter.

Information for Developers
==========================

This repository is providing a git repository of oommf source files and
additional extensions (see above for details).

Updating to new OOMMF releases, or including new extensions
-----------------------------------------------------------

If a new OOMMF release (from NIST) should be included here, one should
use the targets in the `Makefile-update-oommf <Makefile-update-oommf>`__
makefile (see documentation there). The file will need manual updating
(for the new version), before being used. Once it has run through, the
new files need to be committed to the repository. This README might need
updating (where specific version numbers are mentioned).

We have a github workflow to run through the targets in this
`Makefile-update-oommf <Makefile-update-oommf>`__ makefile periodically:

Github workflows
----------------

-  Compile OOMMF on latest Ubuntu on Github’s systems:
   |on-ubuntu-latest|

-  Compile OOMMF on Ubuntu 21:04 (in Docker container): |in-docker|

-  Compile OOMMF on latest OSX: |badge-osx|

-  Replay OOMMF upgrade procedure (see above):
   |in-docker-repeat-oommf-update|

.. |Anaconda-Server Badge| image:: https://anaconda.org/conda-forge/oommf/badges/version.svg
   :target: https://anaconda.org/conda-forge/oommf
.. |License Badge| image:: https://img.shields.io/badge/License-OOMMF-blue.svg
   :target: oommf/LICENSE
.. |on-ubuntu-latest| image:: https://github.com/fangohr/oommf/actions/workflows/on-ubuntu-latest.yml/badge.svg
   :target: https://github.com/fangohr/oommf/actions/workflows/on-ubuntu-latest.yml
.. |in-docker| image:: https://github.com/fangohr/oommf/actions/workflows/in-docker.yml/badge.svg
   :target: https://github.com/fangohr/oommf/actions/workflows/in-docker.yml
.. |badge-osx| image:: https://github.com/fangohr/oommf/actions/workflows/on-osx-latest.yml/badge.svg
   :target: https://github.com/fangohr/oommf/actions/workflows/on-osx-latest.yml
.. |in-docker-repeat-oommf-update| image:: https://github.com/fangohr/oommf/actions/workflows/in-docker-repeat-oommf-update.yml/badge.svg
   :target: https://github.com/fangohr/oommf/actions/workflows/in-docker-repeat-oommf-update.yml
