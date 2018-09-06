#!/bin/bash -x

#################################################################
# set environment specific to  CruiseControl build
#################################################################

# prevent annoying deb# BUILD_TYPE is normally release for CC
export BUILD_TYPE=release
#export BUILD_TYPE=debug

# prevent annoying debug messages
unset MALLOC_CHECK_

# standard install location
export INSTALL_BASE=/build/share

# compiler depends on OS version
OS=$(uname -r | sed 's/^.*\(el[0-9]\+\).*$/\1/')
if [[ "${OS}" == "el5" ]]; then
   # use "nyfix" gcc
   export GCC_ROOT=/opt/nyfix/gcc/4.3.3
   export CC=${GCC_ROOT}/bin/gcc
   export CXX=${GCC_ROOT}/bin/g++
else
   export CC=$(which gcc)
   export CXX=$(which g++)
fi

# needed to prevent multiple definition errors with -std=gnu99" (see BUS-1798)
export CFLAGS=-fgnu89-inline

# cmake
export PATH=/build/share/cmake/${CMAKE_VERSION}/bin:$PATH

