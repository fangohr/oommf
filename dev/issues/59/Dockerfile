# Container to host OOMMF. Modified for solving
# issue https://github.com/fangohr/oommf/issues/59
#
FROM ubuntu:22.04

# Avoid asking for geographic data when installing tzdata.
ENV DEBIAN_FRONTEND noninteractive

# install OOMMF dependencies and git
RUN apt-get -y update && \
    apt-get install -y git tk-dev tcl-dev && \
    rm -rf /var/lib/apt/lists/*

# OOMMF cannot be built as root user.
RUN adduser oommfuser
RUN mkdir /io  # Create working directory for mounting.
RUN chown -R oommfuser /io  # Make sure it is owned by the user.
# become the oommfuser for the future
USER oommfuser

# cd into mounting directory
WORKDIR /io

# Quick sanity check (not needed)
RUN cat /etc/issue

