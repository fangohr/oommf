# OOMMF Docker file

## Creating the image

The makefile can be used to build a new image, and push it to hub docker.

## Using the image

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


## No graphical user interface

Note that OOMMF's graphical user interface cannot be used (without further work). It can be used to execute mif files, through boxsi, for example:

    root@715477218aac:/io# oommf boxsi 

