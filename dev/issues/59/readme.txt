Setup linux on ARM64 using Docker
---------------------------------

- install docker and start Docker, so that `docker --version` reports a version
  rather than a `command not found`.

  Might work with `brew install --cask docker`

- checkout the right branch: `git checkout add-configuration-for-linux-arm64` from this oommf repository.

- build the docker image by going to this folder (`dev/issues/59`) and run `make build`

- clone the repository (again) into a subfolder in `dev/issues/59` by running
  `make clonerepo`. This should checkout the right branch (again:
  `add-configuration-for-linux-arm64`) so that the partially modified OOMMF
  files are available. It will create a subdirectory `repo` which is the one to
  experiment with. Files in this `repo` directory will be seen inside the
  container but can be modified from outside the container (and are thus also
  persistent across docker sessions).

- start the docker container using `make run`. We are now inside the container,
  and have the repository in the subdirectory `repo`. The subdirectory
  `repo/oommf` contains the normal sources. Hans would use `cd repo && make
  build` to trigger the OOMMF compilation, but if you are familiar with oommf
  you would probably use pimake commands in `repo/oommf`.

  Example::

     oommfuser@7894d7d330a4:/io/repo/oommf$ tclsh oommf.tcl +platform
     OOMMF release 2.0b0
     Platform Name:           linux-arm64
     Tcl name for OS:         Linux 5.15.49-linuxkit
     C++ compiler:            /usr/bin/g++
     Shell details ---        
      tclsh (running):        /usr/bin/tclsh
                              (links to /usr/bin/tclsh8.6)
                              --> Version 8.6.12, 64 bit, threaded
      tclsh (OOMMF):          /usr/bin/tclsh8.6
                              --> Version 8.6.12, 64 bit, threaded
      tclConfig.sh:           /usr/lib/aarch64-linux-gnu/tclConfig.sh
                              --> Version 8.6.12
      wish (OOMMF):           /usr/bin/wish8.6
      tkConfig.sh:            /usr/lib/aarch64-linux-gnu/tkConfig.sh
                              --> Tk Version 8.6.12
     OOMMF threads:           Yes: Default thread count = 4
     OOMMF API index:         20181207
     Temp file directory:     /tmp


