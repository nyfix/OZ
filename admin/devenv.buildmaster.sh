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

# use system default compiler (gcc 4.8.5 on el7)
export CC=$(which gcc)
export CXX=$(which g++)
fi

# indicate to devenv.sh NOT to source devenv.common.sh
return 255
