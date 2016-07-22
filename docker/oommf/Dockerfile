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
RUN mkdir /io
WORKDIR /io
ENV OOMMFPATH /usr/local/oommf/oommf/oommf.tcl




