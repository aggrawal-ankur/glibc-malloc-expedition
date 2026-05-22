FROM debian:trixie

RUN apt-get update && apt-get upgrade -y && apt-get install -y \
  build-essential \
  gawk bison \
  texinfo sed \
  gcc gdb \
  git python3 \
  vim nano \
  wget

RUN git clone --shallow-since=2025-12-01 https://sourceware.org/git/glibc.git /glibc && \
  cd /glibc && \
  git checkout glibc-2.43

RUN mkdir /glibc-build && cd /glibc-build && \
  /glibc/configure \
    --prefix=/opt/glibc-2.43 \
    --disable-werror && \
    make -j$(nproc) && \
    make install

# Get the setup file from the repository
RUN cd / && wget https://raw.githubusercontent.com/aggrawal-ankur/systems-dives/refs/heads/main/glibc-malloc/dynamic-analysis-code/setup

# Execute the setup file
RUN chmod u+x setup && . ./setup
