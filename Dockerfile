FROM ubuntu:18.04

# Avoid asking for geographic data when installing tzdata
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get -y update
RUN apt-get install -y git tk-dev tcl-dev

# Compile OOMMF.
WORKDIR /opt/
COPY . oommf/
WORKDIR /opt/oommf

# Add user.
RUN groupadd -g 999 oommfuser && \
    useradd -r -u 999 -g oommfuser oommfuser

# Change owner for oommf directory
RUN chown -R oommfuser:oommfuser /opt/oommf/

USER oommfuser

RUN make build-with-dmi-extension-all

# Create OOMMFTCL environment variable.
ENV OOMMFTCL /opt/oommf/oommf/oommf.tcl
