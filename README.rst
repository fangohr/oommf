===============================================
Object Oriented MicroMagnetic Framework (OOMMF)
===============================================

+---------------------------------------------------------+-----------------------------------+
|Description                                              | Badge                             |
+=========================================================+===================================+
|Build (this repo) on 'latest Ubuntu' (Github workflow)   | |on-ubuntu-latest|                |
+---------------------------------------------------------+-----------------------------------+
|Build (this repo) on Ubuntu 22.04 in Docker container    | |on-ubuntu-in-docker|             |
+---------------------------------------------------------+-----------------------------------+
|Build (this repo) on OSX (Github workflow)               | |badge-osx|                       |
+---------------------------------------------------------+-----------------------------------+
|Release on conda-forge                                   | |Anaconda-Server Badge|           |
+---------------------------------------------------------+-----------------------------------+
|oommf/oommf Docker image on Dockerhub                    | |oommf-oommf-docker-image|        |
+---------------------------------------------------------+-----------------------------------+
|License                                                  | |License Badge|                   |
+---------------------------------------------------------+-----------------------------------+

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

2. include additional user-contributed OOMMF extensions, which have not made it
   into the official distribution from NIST into this git repository. Those can
   be seen from the `Makefile-update-oommf <Makefile-update-oommf>`__. See also
   `Extensions <#Extensions>`__ below.

Compilation of OOMMF (for example by running ``make build``) is expected to work
on Linux and OS X.

Version
-------

The current version of OOMMF code in this repository is the beta
release of OOMMF 2.0 (``20b0 20220930 b0``).

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

   cd oommf
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
3. D2d - antiskyrmion DMI
   (`repository <https://github.com/joommf/oommf-extension-dmi-d2d>`__)

and

4. the OOMMF Magnetoelastic Extension Module
   (`repository <https://github.com/yuyahagi/oommf-mel>`__) from Yu Yahagi.

Docker
------

In the `docker <docker/>`__ directory we provide
`Dockerfile <docker/Dockerfile>`__ and `Makefile <docker/Makefile>`__
for building and running Docker images containg OOMMF. In addition, we
also provide pre-built images at DockerHub under the
`oommf/oommf <https://hub.docker.com/r/oommf/oommf/>`__
repository (`available container images <https://hub.docker.com/r/oommf/oommf/tags>`__).
More information about Docker as well as on how to install
it on your system can be found `here <https://www.docker.com/>`__. This
Docker image is also used by
`Ubermag <https://ubermag.github.io/installation.html#how-does-ubermag-find-oommf>`__.

Getting the Docker image
~~~~~~~~~~~~~~~~~~~~~~~~

If you want to build an image yourself, navigate to the `docker <docker>`__
directory and run

::

   make build

This command builds the image under the ``oommf/oommf:latest`` name.
Otherwise, you can obtain the most recent image by pulling it from
DockerHub
`oommf/oommf <https://hub.docker.com/r/oommf/oommf/>`__
repository

::

   docker pull oommf/oommf

Using the Docker container
~~~~~~~~~~~~~~~~~~~~~~~~~~

To run a container, navigate to `docker <docker>`__ directory and run

::

   make run

Or you can type the full command yourself:

::

   docker run --rm -ti -v `pwd`:/io oommf/oommf:latest 

Once inside the container, the ``oommf.tcl`` file is in
``/usr/local/oommf/oommf/oommf.tcl``. For convenience, we provide a
shell script ``oommf`` in the search path (in ``/usr/local/bin``). This
can be used, for example:

::

   oommfuser@715477218aac:/io# oommf +version
   <7> oommf.tcl 2.0a1  info:
   oommf.tcl 2.0a1

You can also add ``oommf`` commands to the command line from the host, for
example:

::

   docker run --rm -ti -v `pwd`:/io oommf/oommf:latest oommf +version
   <7> oommf.tcl 2.0a1  info:
   oommf.tcl 2.0a1

How can I exchange files between the host and the container?

- The current working directory in the host is shared with the ``/io`` directory in the container.
- This can be used to exchange ``mif`` files and data files with the container environment. For example:

  - create the ``mif`` file on the host
  - then run docker to tell OOMMF to process the ``mif`` file and create data files in the process
  - then analyse data files on the host.


Environment variables in the image 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

During the build process of the container, we also set an environment variable
``OOMMFTCL`` to point to the ``/usr/local/oommf/oommf/oommf.tcl`` file. This is
used by Ubermag, for example, to find the ``oommf.tcl`` file.

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

The License for the OOMMF code can be found `here <oommf/LICENSE>`__. If you use
any of the following extensions (which do not come with the OOMMF code from NIST
at the moment), please refer to the licenses in the corresponding repositories
(`Cnv <https://github.com/joommf/oommf-extension-dmi-cnv>`__, `T(O)
<https://github.com/joommf/oommf-extension-dmi-t>`__, `D2d
<https://github.com/joommf/oommf-extension-dmi-d2d>`__, and `magnetoelastic
<https://github.com/yuyahagi/oommf-mel>`__).

How to cite
-----------

- Information on how to cite the *usage of OOMMF* can be found
  `here <https://math.nist.gov/oommf/oommf_cites.html>`__.

- If you want to acknowledge the *packaging of OOMMF as a git repository, a
  Docker container, a conda-forge package or the use of Ubermag*, please cite
  this paper:

  M. Beg, M. Lang and H. Fangohr,
  *“Ubermag: Towards more effective micromagnetic workflows,”*
  in `IEEE Transactions on Magnetics, DOI: 10.1109/TMAG.2021.3078896
  <https://doi.org/10.1109/TMAG.2021.3078896>`__ (2021)
  
  BibTeX snippet::
  
      @article{beg2021,
        author = {Beg, Marijan and Lang, Martin and Fangohr, Hans},
        journal = {IEEE Transactions on Magnetics},
        title = {Ubermag: Towards more effective micromagnetic workflows},
        year = {2021},
        volume = {},
        number = {},
        pages = {1-1},
        doi = {10.1109/TMAG.2021.3078896}
      }

- If you use *any of the DMI extensions* in your research, please
  refer to the “How to cite” section in the corresponding repositories for
  the particular crystallographic class
  (`Cnv <https://github.com/joommf/oommf-extension-dmi-cnv>`__,
  `T(O) <https://github.com/joommf/oommf-extension-dmi-t>`__, or
  `D2d <https://github.com/joommf/oommf-extension-dmi-d2d>`__).



Acknowledgements
----------------

The repository which holds the OOMMF source code was developed as a part of
`OpenDreamKit <http://opendreamkit.org/>`__ – Horizon 2020 European Research
Infrastructure project (676541) and the `EPSRC Programme grant on Skyrmionics
(EP/N032128/1) <https://www.skyrmions.ac.uk/>`__. Further thanks go to the
`University of Southampton <https://www.soton.ac.uk>`__ and the `Max Planck
Institute for the Structure and Dynamics of Matter <https://mpsd.mpg.de>`__.

Information for Developers
==========================

This repository is providing a git repository of OOMMF source files and
additional extensions (see above for details).

Updating to new OOMMF releases, or including new extensions
-----------------------------------------------------------

If a new OOMMF release (from NIST) should be included here, one should
use the targets in the `Makefile-update-oommf <Makefile-update-oommf>`__
makefile (see comments in makefile). They will (in summary):

1. Using ``make -f Makefile-update-oommf fetch-oommf`` fetch new OOMMF sources from NIST.
   The Makefile will update the
   `versionlog.txt <versionlog.txt>`__ and `oommf-version <oommf-version>`__
   files so that these reflect the new version automatically.
2. Using ``make -f Makefile-update-oommf fetch-all-extensions`` fetch extensions.
   The script `clone-log-and-extract-src.py
   <clone-log-and-extract-src.py>`__ is used to copy the relevant files from
   each extension into the right place with the OOMMF directory structure
   (`oommf/app/oxs/local/ <oommf/app/oxs/local/>`__) into a dedicated
   subdirectory. The script will also add a ``NAME.log`` and ``NAME-HEAD.zip``
   file for each extensions with name ``NAME`` into the relevant subdirectory.
   These files contain additional information about the extension (to provide
   better provenance and reproducibility).

The `Makefile <Makefile-update-oommf>`__ will need manual updating (for example
new version number, ...) before being used in steps 1 and 2.

A recommendation is to:

- iterate through steps 1 and 2 until everything works automatically. You can
  use ``make -f Makefile-update-oommf clean`` to remove all downloaded and
  extracted files before starting over.
- then run ``make build`` to build the new OOMMF and ``make test-all`` to run
  standard problem 3 and 4 as a smoke test. Iterate 1 and 2 until the tests pass.

3. At that point, the modifications to ``Makefile-update-oommf`` should be
   committed to the repository (other modifications of scripts as well if any
   were necessary).

4. Remove all generated files (such as `*.o`) using
   ``make -f Makefile-update-oommf clean``,
   remove all modifications using ``git checkout .``, and
   repeat steps 1 and 2 (so that we only have those files in the directories
   that we like to commit to this repository as part of the new released).

Once this is done, one can

5. commit all of the retrieved (and newly created ``zip`` and ``log``) files to
   this repository

6. make a new release for this repository (for example using Github GUI)
 
7. if desired, push a new docker image to docker hub (see `docker/Makefile
   <docker/Makefile>`__). Note that the current `docker/Dockerfile
   <docker/Dockerfile>`__ uses the OOMMF version on the master branch of this
   repository: if a new OOMMF version is added through a pull request, this
   needs to be merged before the Dockerhub image is built.

8. if desired, update other packaging systems providing OOMMF, for example
   `conda-forge <https://github.com/conda-forge/oommf-feedstock>`__, and
   `spack <https://spack.io/>`__
   (`package.py <https://github.com/spack/spack/blob/develop/var/spack/repos/builtin/packages/oommf/package.py>`__)

This `README <README.rst>`__ will need updating (where specific version numbers are mentioned).

Even though steps 1 and 2 above only need to be exercised where there is a new
OOMMF-version (or new extensions to include), we have a 
`github workflow <.github/workflows/in-docker-repeat-oommf-update.yml>`__ to run
through the targets in this `Makefile-update-oommf <Makefile-update-oommf>`__
makefile periodically. 

Github workflows
----------------

-  Compile OOMMF on latest Ubuntu on Github’s systems:
   |on-ubuntu-latest|

-  Compile OOMMF on Ubuntu 21:04 (in Docker container): |on-ubuntu-in-docker|

-  Compile OOMMF on latest OSX on Github's systems: |badge-osx|

-  Replay OOMMF upgrade procedure (see above):
   |in-docker-repeat-oommf-update|
   
-  Building `Dockerhub image <https://hub.docker.com/u/oommf/oommf>`__ used by
   Ubermag: |ubermag-oommf-docker-image-status|

.. |Anaconda-Server Badge| image:: https://anaconda.org/conda-forge/oommf/badges/version.svg
   :target: https://anaconda.org/conda-forge/oommf
.. |License Badge| image:: https://img.shields.io/badge/License-OOMMF-blue.svg
   :target: oommf/LICENSE
.. |on-ubuntu-latest| image:: https://github.com/fangohr/oommf/actions/workflows/on-ubuntu-latest.yml/badge.svg
   :target: https://github.com/fangohr/oommf/actions/workflows/on-ubuntu-latest.yml
.. |on-ubuntu-in-docker| image:: https://github.com/fangohr/oommf/actions/workflows/on-ubuntu-in-docker.yml/badge.svg
   :target: https://github.com/fangohr/oommf/actions/workflows/on-ubuntu-in-docker.yml
.. |badge-osx| image:: https://github.com/fangohr/oommf/actions/workflows/on-osx-latest.yml/badge.svg
   :target: https://github.com/fangohr/oommf/actions/workflows/on-osx-latest.yml
.. |in-docker-repeat-oommf-update| image:: https://github.com/fangohr/oommf/actions/workflows/in-docker-repeat-oommf-update.yml/badge.svg
   :target: https://github.com/fangohr/oommf/actions/workflows/in-docker-repeat-oommf-update.yml
.. |oommf-oommf-docker-image| image:: https://img.shields.io/badge/Dockerhub-Image-blue.svg
   :target: https://hub.docker.com/r/oommf/oommf
.. |ubermag-oommf-docker-image-status| image:: https://github.com/fangohr/oommf/actions/workflows/ubermag-container.yml/badge.svg
   :target: https://github.com/fangohr/oommf/actions/workflows/ubermag-container.yml
