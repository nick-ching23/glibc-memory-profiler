#!/bin/bash

# Match upstream compilation options.
# https://sources.debian.org/src/glibc/2.41-9/debian/rules
unset CPPFLAGS CFLAGS CXXFLAGS LDFLAGS
export CPPFLAGS=""
export CFLAGS="-O2 -g"
export CXXFLAGS="-O2 -g"
export LDFLAGS=""

rm -rf glibc-build glibc-install
mkdir -p glibc-build glibc-install
GLIBC_INSTALL=$(realpath ./glibc-install)

cd glibc-build
../glibc-src/configure --prefix="$GLIBC_INSTALL" --disable-werror

make -j"$(nproc)"
make install
