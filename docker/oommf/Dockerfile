FROM ubuntu:16.04

RUN apt-get -y update 
RUN apt-get install -y git tk-dev tcl-dev

# Compile oommf and create OOMMFTCL environment variable
WORKDIR /usr/local
RUN git clone http://github.com/fangohr/oommf
WORKDIR /usr/local/oommf
RUN make
ENV OOMMFTCL /usr/local/oommf/oommf/oommf.tcl

# Create executable oommf script
WORKDIR /usr/local/bin
RUN echo "#! /bin/bash" > oommf
RUN echo "tclsh /usr/local/oommf/oommf/oommf.tcl \"\$@\"" >> oommf
RUN chmod a+x oommf

# Enable getting oommf version
WORKDIR /usr/local/bin
RUN echo "#! /bin/bash" > oommf-version
RUN echo "# see https://github.com/fangohr/oommf/blob/master/versionlog.txt" >> oommf-version
RUN echo "# for for meaning of version strings." >> oommf-version
RUN echo "cat /usr/local/oommf/oommf-version" >> oommf-version
RUN chmod a+x oommf-version

# Create working directory for mounting.
RUN mkdir /io
WORKDIR /io
