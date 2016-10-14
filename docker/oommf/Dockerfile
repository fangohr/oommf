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

WORKDIR /usr/local/bin
RUN echo "#! /bin/bash" > oommf-version
RUN echo "# see https://github.com/fangohr/oommf/blob/master/versionlog.txt" >> oommf-version
RUN echo "# for for meaning of version strings." >> oommf-version
RUN echo "cat /usr/local/oommf/oommf-version" >> oommf-version
RUN chmod a+x oommf-version


# Create working directory with easy name for mounting this
RUN mkdir /io
WORKDIR /io

# Tell oommf-python where to find oommf.tcl
ENV OOMMFTCL /usr/local/oommf/oommf/oommf.tcl



