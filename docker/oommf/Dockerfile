FROM ubuntu:xenial
RUN apt-get -y update 
RUN apt-get install -y git
RUN apt-get install -y tk-dev tcl-dev 
RUN mkdir -p /usr/local
WORKDIR /usr/local
RUN git clone http://github.com/fangohr/oommf
# compile OOMMF
WORKDIR /usr/local/oommf
RUN make

# Create executable script
WORKDIR /usr/local/bin
RUN echo "#! /bin/bash" > oommf
RUN echo "tclsh /usr/local/oommf/oommf/oommf.tcl \$@" > oommf
RUN chmod a+x oommf

# Create working directory with easy name for mounting this
RUN mkdir /io
WORKDIR /io

# Tell oommf-python where to find oommf.tcl
ENV OOMMFPATH /usr/local/oommf/oommf/oommf.tcl



