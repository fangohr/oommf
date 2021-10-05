# Container to host OOMMF
#
# Takes the most recent version on https://github.com/fangohr/oommf.git
FROM ubuntu:21.04

# Avoid asking for geographic data when installing tzdata.
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update && \
    apt-get install -y git tk-dev tcl-dev && \
    rm -rf /var/lib/apt/lists/*

# OOMMF cannot be built as root user.
RUN adduser oommfuser
RUN chown -R oommfuser /usr/local  # directory where OOMMF is built.
RUN mkdir /io  # Create working directory for mounting.
RUN chown -R oommfuser /io  # Make sure it is owned by the user.
USER oommfuser

# Compile OOMMF and create OOMMFTCL environment variable
WORKDIR /usr/local
RUN git clone --depth 1 https://github.com/fangohr/oommf.git
WORKDIR /usr/local/oommf

# This Makefile target builds oommf
RUN make build

# Create OOMMFTCL environment variable. Can be used by Ubermag to find
# OOMMF
ENV OOMMFTCL /usr/local/oommf/oommf/oommf.tcl
# OOMMF_ROOT can be used by oommf itself (shouldn't be necessary here
# as the oommf sources are in the same place as oommf.tcl, but can also
# be used to locate the provided examples at
# $OOMMF_ROOT/app/oxs/examples/stdprob3.mif for example:
# oommf boxsi +fg $OOMMF_ROOT/app/oxs/examples/stdprob3.mif -exitondone 1
ENV OOMMF_ROOT /usr/local/oommf/oommf
# Create executable OOMMF script.
WORKDIR /usr/local/bin
RUN echo "#! /bin/bash" > oommf
RUN echo "tclsh /usr/local/oommf/oommf/oommf.tcl \"\$@\"" >> oommf
RUN chmod a+x oommf

# Mounting directory.
WORKDIR /io

# Quick sanity check:
RUN oommf +version
RUN oommf +platform
