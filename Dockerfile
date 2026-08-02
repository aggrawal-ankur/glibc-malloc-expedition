FROM debian:trixie

# Install necessary tools
RUN apt-get update && apt-get upgrade -y && apt-get install -y \
  xz-utils \
  build-essential \
  gawk bison \
  texinfo sed \
  gcc gdb \
  git python3 \
  vim nano \
  wget

# Get the tarball for glibc-2.43 release and extract it
RUN wget https://ftp.gnu.org/gnu/glibc/glibc-2.43.tar.xz && \
  tar -xf glibc-2.43.tar.xz

# Build glibc-2.43
RUN mkdir /glibc-build && cd /glibc-build && \
  /glibc-2.43/configure \
    --prefix=/opt/glibc-2.43 \
    --disable-werror && \
    make -j$(nproc) && \
    make install

# Get the setup file from the repository
RUN cd / && wget https://raw.githubusercontent.com/aggrawal-ankur/glibc-malloc-expedition/refs/heads/main/dynamic-analysis/setup

# Execute the setup file
RUN chmod u+x setup && . ./setup
