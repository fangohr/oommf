FROM ubuntu:18.04

# Avoid asking for geographic data when installing tzdata.
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update
RUN apt-get install -y git tk-dev tcl-dev

# OOMMF cannot be built as root user.
RUN adduser oommfuser
RUN chown -R oommfuser /usr/local  # directory where OOMMF is built.
RUN mkdir /io  # Create working directory for mounting.
RUN chown -R oommfuser /io  # Make sure it is owned by the user.
USER oommfuser

# Compile OOMMF and create OOMMFTCL environment variable
WORKDIR /usr/local
RUN git clone https://github.com/fangohr/oommf.git
WORKDIR /usr/local/oommf

# Temp lines.
RUN git pull origin mel
RUN git checkout mel

RUN make build-with-all

# Create OOMMFTCL environment variable.
ENV OOMMFTCL /usr/local/oommf/oommf/oommf.tcl

# Create executable OOMMF script.
WORKDIR /usr/local/bin
RUN echo "#! /bin/bash" > oommf
RUN echo "tclsh /usr/local/oommf/oommf/oommf.tcl \"\$@\"" >> oommf
RUN chmod a+x oommf

# Mounting directory.
WORKDIR /io
