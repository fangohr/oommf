# Container to host compilation of OOMMF in

FROM ubuntu:21.04

# Avoid asking for geographic data when installing tzdata.
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update
RUN apt-get install -y git tk-dev tcl-dev
# Needed only for updating-oommf-sources 
RUN apt-get install -y wget python3 rsync

# OOMMF cannot be built as root user.
WORKDIR /usr/local
COPY . oommf/
RUN adduser oommfuser
RUN chown -R oommfuser /usr/local  # directory where OOMMF is built.
USER oommfuser

WORKDIR /usr/local/oommf
