#!/bin/bash -x

#################################################################
# set environment specific to  CruiseControl build
#################################################################

# BUILD_TYPE is normally release for CC
export BUILD_TYPE=release
#export BUILD_TYPE=debug

# standard install location
export INSTALL_BASE=/build/share

# needs OpenMAMA source files (!?)
OPENMAMA_INSTALL=${INSTALL_BASE}/OpenMAMA/${OPENMAMA_VERSION}/${BUILD_TYPE}
OPENMAMA_SOURCE=${OPENMAMA_INSTALL}/src
LIBZMQ_INSTALL=${INSTALL_BASE}/libzmq/${LIBZMQ_VERSION}/${BUILD_TYPE}

# build depends on OS version
OS=$(uname -r | sed 's/^.*\(el[0-9]\+\).*$/\1/')
if [[ "${OS}" == "el5" ]]; then

   # need cmake for RH5
   export PATH=/build/share/cmake/2.8.12.2/bin:$PATH

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

# indicate to devenv.sh NOT to source devenv.common.sh
return 255
