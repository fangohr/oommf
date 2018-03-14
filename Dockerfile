FROM ubuntu:16.04

RUN apt-get -y update
RUN apt-get install -y git tk-dev tcl-dev

# Compile OOMMF and create OOMMFTCL environment variable
WORKDIR /usr/local
COPY . oommf/
WORKDIR /usr/local/oommf
RUN make build-with-dmi-extension-all

ENV OOMMFTCL /usr/local/oommf/oommf/oommf.tcl

