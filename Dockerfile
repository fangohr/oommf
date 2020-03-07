FROM ubuntu:18.04

# Avoid asking for geographic data when installing tzdata.
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update
RUN apt-get install -y git tk-dev tcl-dev

# OOMMF cannot be built as root user.
RUN adduser oommfuser
RUN chown -R oommfuser /usr/local  # directory where OOMMF is built.
USER oommfuser

# Compile OOMMF.
WORKDIR /usr/local
COPY . oommf/
WORKDIR /usr/local/oommf
RUN make build-with-all

# Create OOMMFTCL environment variable.
ENV OOMMFTCL /usr/local/oommf/oommf/oommf.tcl
