#!/bin/bash -x

#################################################################
# set environment specific to  CruiseControl build
#################################################################

# prevent annoying deb# BUILD_TYPE is normally release for CC
#export BUILD_TYPE=release
export BUILD_TYPE=debug

ug messages
unset MALLOC_CHECK_

# standard install location
export INSTALL_BASE=/build/share

# use "nyfix" gcc
export CC=gcc
export CXX=g++
#export GCC_ROOT=/opt/nyfix/gcc/4.3.3
export GCC_ROOT=/build/share/gcc/4.3.3
export PATH=${GCC_ROOT}/bin:$PATH
export LD_LIBRARY_PATH=${GCC_ROOT}/lib64:$LD_LIBRARY_PATH

# cmake
export PATH=/build/share/cmake/${CMAKE_VERSION}/bin:$PATH

